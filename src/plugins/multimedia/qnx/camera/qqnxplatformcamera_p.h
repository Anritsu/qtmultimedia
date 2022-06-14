/****************************************************************************
**
** Copyright (C) 2016 Research In Motion
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
#ifndef QQNXPLATFORMCAMERA_H
#define QQNXPLATFORMCAMERA_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include "qqnxcamera_p.h"

#include <private/qplatformcamera_p.h>
#include <private/qplatformmediarecorder_p.h>

#include <QtCore/qlist.h>
#include <QtCore/qmutex.h>
#include <QtCore/qurl.h>

#include <deque>
#include <functional>
#include <memory>

QT_BEGIN_NAMESPACE

class QQnxPlatformCameraFrameBuffer;
class QQnxMediaCaptureSession;
class QQnxVideoSink;
class QQnxCameraFrameBuffer;

class QQnxPlatformCamera : public QPlatformCamera
{
    Q_OBJECT
public:
    explicit QQnxPlatformCamera(QCamera *parent);
    ~QQnxPlatformCamera();

    bool isActive() const override;
    void setActive(bool active) override;
    void start();
    void stop();

    void setCamera(const QCameraDevice &camera) override;

    bool setCameraFormat(const QCameraFormat &format) override;

    void setCaptureSession(QPlatformMediaCaptureSession *session) override;

    bool isFocusModeSupported(QCamera::FocusMode mode) const override;
    void setFocusMode(QCamera::FocusMode mode) override;

    void setCustomFocusPoint(const QPointF &point) override;

    void setFocusDistance(float distance) override;

    void zoomTo(float newZoomFactor, float rate = -1.) override;

    void setExposureCompensation(float ev) override;

    int isoSensitivity() const override;
    void setManualIsoSensitivity(int value) override;
    void setManualExposureTime(float seconds) override;
    float exposureTime() const override;

    bool isWhiteBalanceModeSupported(QCamera::WhiteBalanceMode mode) const override;
    void setWhiteBalanceMode(QCamera::WhiteBalanceMode mode) override;
    void setColorTemperature(int temperature) override;

    void setOutputUrl(const QUrl &url);
    void setMediaEncoderSettings(const QMediaEncoderSettings &settings);

    bool startVideoRecording();

    using VideoFrameCallback = std::function<void(const QVideoFrame&)>;
    void requestVideoFrame(VideoFrameCallback cb);

private:
    void updateCameraFeatures();
    void setColorTemperatureInternal(unsigned temp);

    bool isVideoEncodingSupported() const;

    void onFrameAvailable();

    QQnxMediaCaptureSession *m_session = nullptr;
    QQnxVideoSink *m_videoSink = nullptr;

    QCameraDevice m_cameraDevice;

    QUrl m_outputUrl;

    QMediaEncoderSettings m_encoderSettings;

    uint32_t m_minColorTemperature = 0;
    uint32_t m_maxColorTemperature = 0;

    QMutex m_currentFrameMutex;

    std::unique_ptr<QQnxCamera> m_qnxCamera;
    std::unique_ptr<QQnxCameraFrameBuffer> m_currentFrame;

    std::deque<VideoFrameCallback> m_videoFrameRequests;
};

QT_END_NAMESPACE

#endif
