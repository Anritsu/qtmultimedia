// Copyright (C) 2021 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR
// GPL-3.0-only

#include "qffmpegscreencapture_dxgi_p.h"
#include <private/qabstractvideobuffer_p.h>
#include "qvideoframe.h"

#include <qguiapplication.h>
#include <qloggingcategory.h>
#include <qthread.h>
#include <qwaitcondition.h>
#include <qmutex.h>

#include "D3d11.h"
#include "dxgi1_2.h"
#include <system_error>
#include <thread>
#include <chrono>

QT_BEGIN_NAMESPACE

Q_LOGGING_CATEGORY(qLcScreenCaptureDxgi, "qt.multimedia.ffmpeg.screencapturedxgi")

using namespace std::chrono;

class QD3D11TextureVideoBuffer : public QAbstractVideoBuffer
{
public:
    QD3D11TextureVideoBuffer(QWindowsIUPointer<ID3D11Device> &device, std::shared_ptr<QMutex> &mutex,
                             QWindowsIUPointer<ID3D11Texture2D> &texture, QSize size)
        : QAbstractVideoBuffer(QVideoFrame::NoHandle)
        , m_device(device)
        , m_texture(texture)
        , m_ctxMutex(mutex)
        , m_size(size)
    {}

    ~QD3D11TextureVideoBuffer()
    {
        QD3D11TextureVideoBuffer::unmap();
    }

    QVideoFrame::MapMode mapMode() const override
    {
        return m_mapMode;
    }

    MapData map(QVideoFrame::MapMode mode) override
    {
        MapData mapData;
        if (!m_ctx && mode == QVideoFrame::ReadOnly) {
            D3D11_TEXTURE2D_DESC texDesc = {};
            m_texture->GetDesc(&texDesc);
            texDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
            texDesc.Usage = D3D11_USAGE_STAGING;
            texDesc.MiscFlags = 0;
            texDesc.BindFlags = 0;

            HRESULT hr = m_device->CreateTexture2D(&texDesc, nullptr, m_cpuTexture.address());
            if (FAILED(hr)) {
                qCDebug(qLcScreenCaptureDxgi) << "Failed to create texture with CPU access"
                                              << std::system_category().message(hr).c_str();
                qCDebug(qLcScreenCaptureDxgi) << m_device->GetDeviceRemovedReason();
                return {};
            }

            m_device->GetImmediateContext(m_ctx.address());
            m_ctxMutex->lock();
            m_ctx->CopyResource(m_cpuTexture.get(), m_texture.get());

            D3D11_MAPPED_SUBRESOURCE resource = {};
            hr = m_ctx->Map(m_cpuTexture.get(), 0, D3D11_MAP_READ, 0, &resource);
            m_ctxMutex->unlock();
            if (FAILED(hr)) {
                qCDebug(qLcScreenCaptureDxgi) << "Failed to map texture" << m_cpuTexture.get()
                                              << std::system_category().message(hr).c_str();
                return {};
            }

            m_mapMode = mode;
            mapData.nPlanes = 1;
            mapData.bytesPerLine[0] = int(resource.RowPitch);
            mapData.data[0] = reinterpret_cast<uchar*>(resource.pData);
            mapData.size[0] = m_size.height() * int(resource.RowPitch);
        }

        return mapData;
    }

    void unmap() override
    {
        if (m_mapMode == QVideoFrame::NotMapped)
            return;
        if (m_ctx) {
            m_ctxMutex->lock();
            m_ctx->Unmap(m_cpuTexture.get(), 0);
            m_ctxMutex->unlock();
            m_ctx.reset();
        }
        m_cpuTexture.reset();
        m_mapMode = QVideoFrame::NotMapped;
    }

private:
    QWindowsIUPointer<ID3D11Device> m_device;
    QWindowsIUPointer<ID3D11Texture2D> m_texture;
    QWindowsIUPointer<ID3D11Texture2D> m_cpuTexture;
    QWindowsIUPointer<ID3D11DeviceContext> m_ctx;
    std::shared_ptr<QMutex> m_ctxMutex;
    QSize m_size;
    QVideoFrame::MapMode m_mapMode = QVideoFrame::NotMapped;
};

static std::pair<QWindowsIUPointer<ID3D11Texture2D>, QString> getNextFrame(ID3D11Device *dev, IDXGIOutputDuplication *dup)
{
    QWindowsIUPointer<IDXGIResource> frame;
    DXGI_OUTDUPL_FRAME_INFO info;
    HRESULT hr = dup->AcquireNextFrame(0, &info, frame.address());
    if (FAILED(hr))
        return { {},
                 hr == DXGI_ERROR_WAIT_TIMEOUT ? QString{}
                                               : "Failed to grab the screen content"
                                                 + QString(std::system_category().message(hr).c_str())};

    QWindowsIUPointer<ID3D11Texture2D> tex;
    hr = frame->QueryInterface(__uuidof(ID3D11Texture2D), reinterpret_cast<void **>(tex.address()));
    if (FAILED(hr)) {
        dup->ReleaseFrame();
        return { {}, "Failed to obtain D3D11 texture"
                     + QString(std::system_category().message(hr).c_str())};
    }

    D3D11_TEXTURE2D_DESC texDesc = {};
    tex->GetDesc(&texDesc);
    texDesc.MiscFlags = 0;
    texDesc.BindFlags = 0;
    QWindowsIUPointer<ID3D11Texture2D> texCopy;
    hr = dev->CreateTexture2D(&texDesc, nullptr, texCopy.address());
    if (FAILED(hr)) {
        dup->ReleaseFrame();
        return { {}, "Failed to create texture with CPU access"
                          + QString(std::system_category().message(hr).c_str())};
    }

    QWindowsIUPointer<ID3D11DeviceContext> ctx;
    dev->GetImmediateContext(ctx.address());
    ctx->CopyResource(texCopy.get(), tex.get());

    dup->ReleaseFrame();

    return { texCopy, {} };
}

class DxgiScreenGrabberActive : public QThread
{
public:
    DxgiScreenGrabberActive(QPlatformScreenCapture &screenCapture, QWindowsIUPointer<ID3D11Device> &device,
                        QWindowsIUPointer<IDXGIOutputDuplication> &duplication, qreal maxFrameRate)
        : QThread()
        , m_screenCapture(screenCapture)
        , m_duplication(duplication)
        , m_device(device)
        , m_maxFrameRate(maxFrameRate)
    {}

    void run() override
    {
        qCDebug(qLcScreenCaptureDxgi) << "ScreenGrabberActive started";

        DXGI_OUTDUPL_DESC outputDesc = {};
        m_duplication->GetDesc(&outputDesc);

        QSize frameSize = { int(outputDesc.ModeDesc.Width), int(outputDesc.ModeDesc.Height) };
        QVideoFrameFormat format(frameSize, QVideoFrameFormat::Format_BGRA8888);

        m_formatMutex.lock();
        m_format = format;
        m_format.setFrameRate(int(m_maxFrameRate));
        m_formatMutex.unlock();
        m_waitForFormat.wakeAll();

        const microseconds frameTime(quint64(1000000. / m_maxFrameRate));
        time_point frameStopTime = steady_clock::now();
        time_point frameStartTime = frameStopTime - frameTime;

        std::shared_ptr<QMutex> ctxMutex(new QMutex);
        while (!isInterruptionRequested()) {
            ctxMutex->lock();
            auto [tex, err] = getNextFrame(m_device.get(), m_duplication.get());
            ctxMutex->unlock();

            if (tex) {
                auto buffer = new QD3D11TextureVideoBuffer(m_device, ctxMutex, tex, frameSize);
                QVideoFrame frame(buffer, format);
                frame.setStartTime(duration_cast<microseconds>(frameStartTime.time_since_epoch()).count());
                frame.setEndTime(duration_cast<microseconds>(frameStopTime.time_since_epoch()).count());
                emit m_screenCapture.newVideoFrame(frame);
            } else {
                emit m_screenCapture.updateError(err.isEmpty() ? QScreenCapture::NoError : QScreenCapture::CaptureFailed, err);
            }

            frameStartTime = frameStopTime;
            std::this_thread::sleep_until(frameStartTime + frameTime);
            frameStopTime = steady_clock::now();
        }

        qCDebug(qLcScreenCaptureDxgi) << "ScreenGrabberActive finished";
    }

    QVideoFrameFormat format() {
        QMutexLocker locker(&m_formatMutex);
        if (!m_format.isValid())
            m_waitForFormat.wait(&m_formatMutex);
        return m_format;
    }

private:
    QPlatformScreenCapture &m_screenCapture;
    QWindowsIUPointer<IDXGIOutputDuplication> m_duplication;
    QWindowsIUPointer<ID3D11Device> m_device;
    qreal m_maxFrameRate;
    QWaitCondition m_waitForFormat;
    QVideoFrameFormat m_format;
    QMutex m_formatMutex;
};

static std::pair<QWindowsIUPointer<IDXGIOutputDuplication>, QString> duplicateOutput(ID3D11Device* device, IDXGIOutput *output)
{
    QWindowsIUPointer<IDXGIOutput1> output1;
    HRESULT hr = output->QueryInterface(__uuidof(IDXGIOutput1), reinterpret_cast<void**>(output1.address()));
    if (FAILED(hr))
        return  { {} , "Failed to create IDXGIOutput1" + QString(std::system_category().message(hr).c_str()) };

    QWindowsIUPointer<IDXGIOutputDuplication> dup;
    hr = output1->DuplicateOutput(device, dup.address());

    return { dup, SUCCEEDED(hr) ? QString{}
                                : "Failed to duplicate IDXGIOutput1" + QString(std::system_category().message(hr).c_str()) };
}

struct DxgiScreen {
    QWindowsIUPointer<IDXGIAdapter1> adapter;
    QWindowsIUPointer<IDXGIOutput> output;
};

static std::pair<DxgiScreen, QString> findDxgiScreen(const QScreen *screen)
{
    if (!screen)
        return { {}, "Cannot find nullptr screen"  };

    QWindowsIUPointer<IDXGIFactory1> factory;
    HRESULT hr = CreateDXGIFactory1(__uuidof(IDXGIFactory1), reinterpret_cast<void**>(factory.address()));
    if (FAILED(hr))
        return { {}, "Failed to create IDXGIFactory" + QString(std::system_category().message(hr).c_str()) };

    QWindowsIUPointer<IDXGIAdapter1> adapter;
    for (quint32 i = 0; SUCCEEDED(factory->EnumAdapters1(i, adapter.address())); i++, adapter.reset()) {
        QWindowsIUPointer<IDXGIOutput> output;
        for (quint32 j = 0; SUCCEEDED(adapter->EnumOutputs(j, output.address())); j++, output.reset()) {
            DXGI_OUTPUT_DESC desc = {};
            output->GetDesc(&desc);
            qCDebug(qLcScreenCaptureDxgi) << i << j << QString::fromWCharArray(desc.DeviceName);
            if (QString::fromWCharArray(desc.DeviceName) == screen->name())
                return { DxgiScreen{ adapter, output }, {}};
        }
    }
    // TODO: handle user friendly names on screen->name(), see qwindowsscreen.cpp
    return { {}, "Could not find screen adapter" + screen->name() };
}

static std::pair<QWindowsIUPointer<ID3D11Device>, QString> createD3D11Device(IDXGIAdapter1 *adapter)
{
    QWindowsIUPointer<ID3D11Device> d3d11dev;
    HRESULT hr = D3D11CreateDevice(adapter, D3D_DRIVER_TYPE_UNKNOWN, nullptr,
                                   D3D11_CREATE_DEVICE_DEBUG , nullptr, 0, D3D11_SDK_VERSION,
                                   d3d11dev.address(), nullptr, nullptr);

    return { d3d11dev
           , SUCCEEDED(hr) ? QString{}
                           : "Failed to create ID3D11Device device"
                                + QString(std::system_category().message(hr).c_str()) };
}

QFFmpegScreenCaptureDxgi::QFFmpegScreenCaptureDxgi(QScreenCapture *screenCapture)
    : QPlatformScreenCapture(screenCapture)
{
}

QFFmpegScreenCaptureDxgi::~QFFmpegScreenCaptureDxgi() = default;

QVideoFrameFormat QFFmpegScreenCaptureDxgi::format() const
{
    if (m_active)
        return m_active->format();
    else
        return {};
}

void QFFmpegScreenCaptureDxgi::setScreen(QScreen *screen)
{
    QScreen *oldScreen = m_screen;
    if (oldScreen == screen)
        return;

    bool active = bool(m_active);
    if (active)
        setActiveInternal(false);

    m_screen = screen;
    if (active)
        setActiveInternal(true);

    emit screenCapture()->screenChanged(screen);
}

void QFFmpegScreenCaptureDxgi::setActiveInternal(bool active)
{
    if (bool(m_active) == active)
        return;

    if (m_active) {
        m_active->requestInterruption();
        m_active->quit();
        m_active->wait();
        m_active.reset();

    } else {
        QScreen *screen = m_screen ? m_screen : QGuiApplication::primaryScreen();
        auto [dxgiScreen, err0] = findDxgiScreen(screen);
        if (err0.isNull()) {
            auto [dev, err1] = createD3D11Device(dxgiScreen.adapter.get());
            if (dev) {
                auto [dup, err2] = duplicateOutput(dev.get(), dxgiScreen.output.get());
                if (dup) {
                    emit updateError(QScreenCapture::NoError, {});
                    qreal maxFrameRate = screen->refreshRate();
                    m_active.reset(new DxgiScreenGrabberActive(*this, dev, dup, maxFrameRate));
                    m_active->start();
                } else {
                    qCDebug(qLcScreenCaptureDxgi) << err2;
                    emit updateError(QScreenCapture::InternalError, err2);
                }
            } else {
                qCDebug(qLcScreenCaptureDxgi) << err1;
                emit updateError(QScreenCapture::InternalError, err1);
            }
        } else {
            qCDebug(qLcScreenCaptureDxgi) << err0;
            emit updateError(QScreenCapture::NotFound, err0);
        }
    }
}

void QFFmpegScreenCaptureDxgi::setActive(bool active)
{
    if (bool(m_active) == active)
        return;

    setActiveInternal(active);
    emit screenCapture()->activeChanged(active);
}

QT_END_NAMESPACE
