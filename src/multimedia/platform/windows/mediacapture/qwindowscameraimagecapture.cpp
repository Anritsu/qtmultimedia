/****************************************************************************
**
** Copyright (C) 2021 The Qt Company Ltd.
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

#include "qwindowscameraimagecapture_p.h"

#include "qwindowsmediadevicesession_p.h"
#include "qwindowsmediacapture_p.h"

#include <QtConcurrent/qtconcurrentrun.h>
#include <QtGui/qimagewriter.h>

QT_BEGIN_NAMESPACE

QWindowsCameraImageCapture::QWindowsCameraImageCapture(QCameraImageCapture *parent)
    : QPlatformCameraImageCapture(parent)
{
}

QWindowsCameraImageCapture::~QWindowsCameraImageCapture() = default;

bool QWindowsCameraImageCapture::isReadyForCapture() const
{
    if (!m_mediaDeviceSession)
        return false;
    return !m_capturing && m_mediaDeviceSession->isActive() && !m_mediaDeviceSession->activeCamera().isNull();
}

int QWindowsCameraImageCapture::capture(const QString &fileName)
{
    QString ext = writerFormat(m_settings.format());
    QString path = m_storageLocation.generateFileName(fileName, QWindowsStorageLocation::Image,
                                                      QLatin1String("img_"), ext);
    return doCapture(path);
}

int QWindowsCameraImageCapture::captureToBuffer()
{
    return doCapture(QString());
}

int QWindowsCameraImageCapture::doCapture(const QString &fileName)
{
    if (!isReadyForCapture())
        return -1;
    m_fileName = fileName;
    m_capturing = true;
    return m_captureId;
}

QImageEncoderSettings QWindowsCameraImageCapture::imageSettings() const
{
    return m_settings;
}

void QWindowsCameraImageCapture::setImageSettings(const QImageEncoderSettings &settings)
{
    m_settings = settings;
}

void QWindowsCameraImageCapture::setCaptureSession(QPlatformMediaCaptureSession *session)
{
    QWindowsMediaCaptureService *captureService = static_cast<QWindowsMediaCaptureService *>(session);
    if (m_captureService == captureService)
        return;

    if (m_mediaDeviceSession)
        disconnect(m_mediaDeviceSession, nullptr, this, nullptr);

    m_captureService = captureService;
    if (!m_captureService) {
        m_mediaDeviceSession = nullptr;
        return;
    }

    m_mediaDeviceSession = m_captureService->session();
    Q_ASSERT(m_mediaDeviceSession);

    connect(m_mediaDeviceSession, SIGNAL(readyForCaptureChanged(bool)),
            this, SIGNAL(readyForCaptureChanged(bool)));

    connect(m_mediaDeviceSession, SIGNAL(newVideoFrame(QVideoFrame)),
            this, SLOT(handleNewVideoFrame(QVideoFrame)));
}

void QWindowsCameraImageCapture::handleNewVideoFrame(const QVideoFrame &frame)
{
    if (m_capturing) {

        QImage image = frame.toImage();

        emit imageExposed(m_captureId);
        emit imageAvailable(m_captureId, frame);
        emit imageCaptured(m_captureId, image);

        QMediaMetaData metaData = this->metaData();
        metaData.insert(QMediaMetaData::Date, QDateTime::currentDateTime());
        metaData.insert(QMediaMetaData::Resolution, frame.size());

        emit imageMetadataAvailable(m_captureId, metaData);

        if (!m_fileName.isEmpty()) {

            (void)QtConcurrent::run(&QWindowsCameraImageCapture::saveImage, this,
                                    m_captureId, m_fileName, image, metaData, m_settings);
        }

        ++m_captureId;
        m_capturing = false;
    }
}

void QWindowsCameraImageCapture::saveImage(int captureId, const QString &fileName,
                                           const QImage &image, const QMediaMetaData &metaData,
                                           const QImageEncoderSettings &settings)
{
    QImageWriter imageWriter;
    imageWriter.setFileName(fileName);

    QString format = writerFormat(settings.format());
    imageWriter.setFormat(format.toUtf8());

    int quality = writerQuality(format, settings.quality());
    if (quality > -1)
        imageWriter.setQuality(quality);

    for (auto key : metaData.keys())
        imageWriter.setText(QMediaMetaData::metaDataKeyToString(key),
                            metaData.stringValue(key));

    imageWriter.write(image);

    QMetaObject::invokeMethod(this, "imageSaved", Qt::QueuedConnection,
                              Q_ARG(int, captureId), Q_ARG(QString, fileName));
}

QString QWindowsCameraImageCapture::writerFormat(QImageEncoderSettings::FileFormat reqFormat)
{
    QString format;

    switch (reqFormat) {
    case QImageEncoderSettings::FileFormat::JPEG:
        format = QLatin1String("jpg");
        break;
    case QImageEncoderSettings::FileFormat::PNG:
        format = QLatin1String("png");
        break;
    case QImageEncoderSettings::FileFormat::WebP:
        format = QLatin1String("webp");
        break;
    case QImageEncoderSettings::FileFormat::Tiff:
        format = QLatin1String("tiff");
        break;
    default:
        format = QLatin1String("jpg");
    }

    auto supported = QImageWriter::supportedImageFormats();
    for (const auto &f : supported)
        if (format.compare(QString::fromUtf8(f), Qt::CaseInsensitive) == 0)
            return format;

    return QLatin1String("jpg");
}

int QWindowsCameraImageCapture::writerQuality(const QString &writerFormat,
                                              QImageEncoderSettings::Quality quality)
{
    if (writerFormat.compare(QLatin1String("jpg"), Qt::CaseInsensitive) == 0 ||
            writerFormat.compare(QLatin1String("jpeg"), Qt::CaseInsensitive) == 0) {

        switch (quality) {
        case QImageEncoderSettings::Quality::VeryLowQuality:
            return 10;
        case QImageEncoderSettings::Quality::LowQuality:
            return 30;
        case QImageEncoderSettings::Quality::NormalQuality:
            return 75;
        case QImageEncoderSettings::Quality::HighQuality:
            return 90;
        case QImageEncoderSettings::Quality::VeryHighQuality:
            return 98;
        default:
            return 75;
        }
    }
    return -1;
}

QT_END_NAMESPACE
