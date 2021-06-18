/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qandroidcameravideorenderercontrol_p.h"

#include "qandroidcamerasession_p.h"
#include "qandroidvideooutput_p.h"
#include "androidsurfaceview_p.h"
#include "qandroidmultimediautils_p.h"
#include <private/qrhi_p.h>
#include <qvideosink.h>
#include <qvideoframeformat.h>
#include <qcoreapplication.h>
#include <qthread.h>

QT_BEGIN_NAMESPACE

class QAndroidCameraDataVideoOutput : public QAndroidVideoOutput
                                    , public QAndroidCameraSession::PreviewCallback
{
    Q_OBJECT
public:
    explicit QAndroidCameraDataVideoOutput(QAndroidCameraVideoRendererControl *control);
    ~QAndroidCameraDataVideoOutput() override;

    AndroidSurfaceHolder *surfaceHolder() override;

    bool isReady() override;

    void stop() override;

private Q_SLOTS:
    void onSurfaceCreated();
    void configureFormat();

private:
    void onFrameAvailable(const QVideoFrame &frame) override;
    void presentFrame();
    bool event(QEvent *) override;

    QAndroidCameraVideoRendererControl *m_control;
    AndroidSurfaceView *m_surfaceView;
    QMutex m_mutex;
    QVideoFrameFormat::PixelFormat m_pixelFormat;
    QVideoFrame m_lastFrame;
};

QAndroidCameraDataVideoOutput::QAndroidCameraDataVideoOutput(QAndroidCameraVideoRendererControl *control)
    : QAndroidVideoOutput(control)
    , m_control(control)
    , m_pixelFormat(QVideoFrameFormat::Format_Invalid)
{
    // The camera preview cannot be started unless we set a SurfaceTexture or a
    // SurfaceHolder. In this case we don't actually care about either of these, but since
    // we need to, we setup an offscreen dummy SurfaceView in order to be able to start
    // the camera preview. We'll then be able to use setPreviewCallbackWithBuffer() to
    // get the raw data.

    m_surfaceView = new AndroidSurfaceView;

    connect(m_surfaceView, &AndroidSurfaceView::surfaceCreated,
            this, &QAndroidCameraDataVideoOutput::onSurfaceCreated);

    m_surfaceView->setGeometry(-1, -1, 1, 1);
    m_surfaceView->setVisible(true);

    connect(m_control->cameraSession(), &QAndroidCameraSession::opened,
            this, &QAndroidCameraDataVideoOutput::configureFormat);
    configureFormat();
}

QAndroidCameraDataVideoOutput::~QAndroidCameraDataVideoOutput()
{
    m_control->cameraSession()->setPreviewCallback(nullptr);
    delete m_surfaceView;
}

AndroidSurfaceHolder *QAndroidCameraDataVideoOutput::surfaceHolder()
{
    return m_surfaceView->holder();
}

bool QAndroidCameraDataVideoOutput::isReady()
{
    return m_surfaceView->holder() && m_surfaceView->holder()->isSurfaceCreated();
}

void QAndroidCameraDataVideoOutput::onSurfaceCreated()
{
    emit readyChanged(true);
}

void QAndroidCameraDataVideoOutput::configureFormat()
{
    m_pixelFormat = QVideoFrameFormat::Format_Invalid;

    if (!m_control->cameraSession()->camera())
        return;

    QList<AndroidCamera::ImageFormat> previewFormats = m_control->cameraSession()->camera()->getSupportedPreviewFormats();
    for (int i = 0; i < previewFormats.size(); ++i) {
        QVideoFrameFormat::PixelFormat pixFormat = qt_pixelFormatFromAndroidImageFormat(previewFormats.at(i));
        if (pixFormat != QVideoFrameFormat::Format_Invalid) {
            m_pixelFormat = pixFormat;
            break;
        }
    }

    if (m_pixelFormat == QVideoFrameFormat::Format_Invalid) {
        m_control->cameraSession()->setPreviewCallback(nullptr);
        qWarning("The video surface is not compatible with any format supported by the camera");
    } else {
        m_control->cameraSession()->setPreviewCallback(this);

        if (m_control->cameraSession()->isActive())
            m_control->cameraSession()->camera()->stopPreview();

        m_control->cameraSession()->setPreviewFormat(qt_androidImageFormatFromPixelFormat(m_pixelFormat));

        if (m_control->cameraSession()->isActive())
            m_control->cameraSession()->camera()->startPreview();
    }
}

void QAndroidCameraDataVideoOutput::stop()
{
    m_mutex.lock();
    m_lastFrame = QVideoFrame();
    m_mutex.unlock();
}

void QAndroidCameraDataVideoOutput::onFrameAvailable(const QVideoFrame &frame)
{
    m_mutex.lock();
    m_lastFrame = frame;
    m_mutex.unlock();

    if (thread() == QThread::currentThread())
        presentFrame();
    else
        QCoreApplication::postEvent(this, new QEvent(QEvent::User), Qt::HighEventPriority);
}

bool QAndroidCameraDataVideoOutput::event(QEvent *e)
{
    if (e->type() == QEvent::User) {
        presentFrame();
        return true;
    }

    return QObject::event(e);
}

void QAndroidCameraDataVideoOutput::presentFrame()
{
    Q_ASSERT(thread() == QThread::currentThread());

    QMutexLocker locker(&m_mutex);

    if (m_control->surface() && m_lastFrame.isValid() && m_lastFrame.pixelFormat() == m_pixelFormat) {

        QVideoFrameFormat format(m_lastFrame.surfaceFormat());
        // Front camera frames are automatically mirrored when using SurfaceTexture or SurfaceView,
        // but the buffers we get from the data callback are not. Tell the QAbstractVideoSurface
        // that it needs to mirror the frames.
        if (m_control->cameraSession()->camera()->getFacing() == AndroidCamera::CameraFacingFront)
            format.setMirrored(true);

        // #### set mirrored on frame

        m_control->surface()->newVideoFrame(m_lastFrame);
    }

    m_lastFrame = QVideoFrame();
}


QAndroidCameraVideoRendererControl::QAndroidCameraVideoRendererControl(QAndroidCameraSession *session, QObject *parent)
    : QObject(parent)
    , m_cameraSession(session)
    , m_surface(0)
    , m_textureOutput(0)
    , m_dataOutput(0)
{
}

QAndroidCameraVideoRendererControl::~QAndroidCameraVideoRendererControl()
{
    m_cameraSession->setVideoOutput(0);
}

QVideoSink *QAndroidCameraVideoRendererControl::surface() const
{
    return m_surface;
}

void QAndroidCameraVideoRendererControl::setSurface(QVideoSink *surface)
{
    if (m_surface == surface)
        return;

    m_surface = surface;
    QAndroidVideoOutput *oldOutput = m_textureOutput ? static_cast<QAndroidVideoOutput*>(m_textureOutput)
                                                     : static_cast<QAndroidVideoOutput*>(m_dataOutput);
    QAndroidVideoOutput *newOutput = 0;

    if (m_surface) {
        if (m_surface->rhi() && m_surface->rhi()->backend() == QRhi::OpenGLES2) {
            if (!m_textureOutput) {
                m_dataOutput = 0;
                newOutput = m_textureOutput = new QAndroidTextureVideoOutput(this);
            }
        } else if (!m_dataOutput) {
            m_textureOutput = 0;
            newOutput = m_dataOutput = new QAndroidCameraDataVideoOutput(this);
        }

        if (m_textureOutput)
            m_textureOutput->setSurface(m_surface);
    }

    if (newOutput != oldOutput) {
        m_cameraSession->setVideoOutput(newOutput);
        delete oldOutput;
    }
}

QT_END_NAMESPACE

#include "qandroidcameravideorenderercontrol.moc"

