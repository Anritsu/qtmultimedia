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

#include "qgstreamermediaencoder_p.h"
#include "private/qgstreamerintegration_p.h"
#include "private/qgstreamerformatinfo_p.h"
#include "private/qgstpipeline_p.h"
#include "private/qgstreamermessage_p.h"
#include "qaudiodevice.h"

#include <qdebug.h>
#include <qstandardpaths.h>
#include <qmimetype.h>
#include <qloggingcategory.h>

#include <gst/gsttagsetter.h>
#include <gst/gstversion.h>
#include <gst/video/video.h>
#include <gst/pbutils/encoding-profile.h>

Q_LOGGING_CATEGORY(qLcMediaEncoder, "qt.multimedia.encoder")

QGstreamerMediaEncoder::QGstreamerMediaEncoder(QMediaRecorder *parent)
  : QPlatformMediaEncoder(parent)
{
    gstEncoder = QGstElement("encodebin", "encodebin");
    gstFileSink = QGstElement("filesink", "filesink");
    gstFileSink.set("location", "dummy");
}

QGstreamerMediaEncoder::~QGstreamerMediaEncoder()
{
    gstPipeline.removeMessageFilter(this);
    gstPipeline.setStateSync(GST_STATE_NULL);
    QObject::disconnect(cameraChanged);
}

QUrl QGstreamerMediaEncoder::outputLocation() const
{
    return m_requestedOutputLocation;
}

bool QGstreamerMediaEncoder::setOutputLocation(const QUrl &sink)
{
    m_requestedOutputLocation = sink;
    return true;
}

void QGstreamerMediaEncoder::updateStatus()
{
    static QMediaRecorder::Status statusTable[3][3] = {
        //Stopped recorder state:
        { QMediaRecorder::StoppedStatus, QMediaRecorder::FinalizingStatus, QMediaRecorder::FinalizingStatus },
        //Recording recorder state:
        { QMediaRecorder::StartingStatus, QMediaRecorder::RecordingStatus, QMediaRecorder::PausedStatus },
        //Paused recorder state:
        { QMediaRecorder::StartingStatus, QMediaRecorder::RecordingStatus, QMediaRecorder::PausedStatus }
    };

    QMediaRecorder::RecorderState sessionState = QMediaRecorder::StoppedState;

    auto gstState = gstEncoder.isNull() ? GST_STATE_NULL : gstEncoder.state();
    switch (gstState) {
    case GST_STATE_PLAYING:
        sessionState = QMediaRecorder::RecordingState;
        break;
    case GST_STATE_PAUSED:
        sessionState = QMediaRecorder::PausedState;
        break;
    default:
        sessionState = QMediaRecorder::StoppedState;
        break;
    }

    auto newStatus = statusTable[state()][sessionState];

    qCDebug(qLcMediaEncoder) << "updateStatus" << state() << sessionState << newStatus;
    statusChanged(newStatus);
}

void QGstreamerMediaEncoder::handleSessionError(QMediaRecorder::Error code, const QString &description)
{
    error(code, description);
    stop();
}

bool QGstreamerMediaEncoder::processBusMessage(const QGstreamerMessage &message)
{
    GstMessage *gm = message.rawMessage();
    if (!gm)
        return false;

//    qCDebug(qLcMediaEncoder) << "received event from" << message.source().name() << Qt::hex << message.type();
//    if (message.type() == GST_MESSAGE_STATE_CHANGED) {
//        GstState    oldState;
//        GstState    newState;
//        GstState    pending;
//        gst_message_parse_state_changed(gm, &oldState, &newState, &pending);
//        qCDebug(qLcMediaEncoder) << "received state change from" << message.source().name() << oldState << newState << pending;
//    }
    if (message.type() == GST_MESSAGE_ELEMENT) {
        QGstStructure s = gst_message_get_structure(gm);
        qCDebug(qLcMediaEncoder) << "received element message from" << message.source().name() << s.name();
        if (s.name() == "GstBinForwarded")
            gm = s.getMessage();
        if (!gm)
            return false;
    }

    if (GST_MESSAGE_TYPE(gm) == GST_MESSAGE_EOS) {
        qCDebug(qLcMediaEncoder) << "received EOS from" << QGstObject(GST_MESSAGE_SRC(gm)).name();
        finalize();
        return false;
    }

    if (GST_MESSAGE_TYPE(gm) == GST_MESSAGE_ERROR) {
        GError *err;
        gchar *debug;
        gst_message_parse_error(gm, &err, &debug);
        error(QMediaRecorder::ResourceError, QString::fromUtf8(err->message));
        g_error_free(err);
        g_free(debug);
    }

    if (GST_MESSAGE_SRC(gm) == gstEncoder.object()) {
        switch (GST_MESSAGE_TYPE(gm))  {
        case GST_MESSAGE_STATE_CHANGED: {
            GstState    oldState;
            GstState    newState;
            GstState    pending;
            gst_message_parse_state_changed(gm, &oldState, &newState, &pending);

            if (newState == GST_STATE_PAUSED && !m_metaData.isEmpty())
                setMetaData(m_metaData);
            updateStatus();
            break;
        }
        default:
            break;
        }
    }
    return false;
}

void QGstreamerMediaEncoder::updateDuration()
{
    durationChanged(m_duration.elapsed());
}

qint64 QGstreamerMediaEncoder::duration() const
{
    return m_duration.elapsed();
}


static GstEncodingContainerProfile *createContainerProfile(const QMediaEncoderSettings &settings)
{
    auto *formatInfo = QGstreamerIntegration::instance()->m_formatsInfo;

    QGstMutableCaps caps = formatInfo->formatCaps(settings.fileFormat());

    GstEncodingContainerProfile *profile = (GstEncodingContainerProfile *)gst_encoding_container_profile_new(
        "container_profile",
        (gchar *)"custom container profile",
        const_cast<GstCaps *>(caps.get()),
        nullptr); //preset
    return profile;
}

static GstEncodingProfile *createVideoProfile(const QMediaEncoderSettings &settings)
{
    auto *formatInfo = QGstreamerIntegration::instance()->m_formatsInfo;

    QGstMutableCaps caps = formatInfo->videoCaps(settings.mediaFormat());
    if (caps.isNull())
        return nullptr;

    GstEncodingVideoProfile *profile = gst_encoding_video_profile_new(
        const_cast<GstCaps *>(caps.get()),
        nullptr,
        nullptr, //restriction
        0); //presence

    gst_encoding_video_profile_set_pass(profile, 0);
    gst_encoding_video_profile_set_variableframerate(profile, TRUE);

    return (GstEncodingProfile *)profile;
}

static GstEncodingProfile *createAudioProfile(const QMediaEncoderSettings &settings)
{
    auto *formatInfo = QGstreamerIntegration::instance()->m_formatsInfo;

    auto caps = formatInfo->audioCaps(settings.mediaFormat());
    if (caps.isNull())
        return nullptr;

    GstEncodingProfile *profile = (GstEncodingProfile *)gst_encoding_audio_profile_new(
        const_cast<GstCaps *>(caps.get()),
        nullptr, //preset
        nullptr,   //restriction
        0);     //presence

    return profile;
}


static GstEncodingContainerProfile *createEncodingProfile(const QMediaEncoderSettings &settings)
{
    auto *containerProfile = createContainerProfile(settings);
    if (containerProfile) {
        GstEncodingProfile *audioProfile = createAudioProfile(settings);
        GstEncodingProfile *videoProfile = settings.videoCodec() != QMediaFormat::VideoCodec::Unspecified ? createVideoProfile(settings) : nullptr;
//        qDebug() << "audio profile" << gst_caps_to_string(gst_encoding_profile_get_format(audioProfile));
//        qDebug() << "video profile" << gst_caps_to_string(gst_encoding_profile_get_format(videoProfile));
//        qDebug() << "conta profile" << gst_caps_to_string(gst_encoding_profile_get_format((GstEncodingProfile *)containerProfile));

        if (videoProfile) {
            if (!gst_encoding_container_profile_add_profile(containerProfile, videoProfile))
                gst_encoding_profile_unref(videoProfile);
        }
        if (audioProfile) {
            if (!gst_encoding_container_profile_add_profile(containerProfile, audioProfile))
                gst_encoding_profile_unref(audioProfile);
        }
    }

    return containerProfile;
}

void QGstreamerMediaEncoder::setState(QMediaRecorder::RecorderState s)
{
    if (s == state())
        return;

    switch (s) {
    case QMediaRecorder::StoppedState:
        stop();
        break;
    case QMediaRecorder::PausedState:
        pause();
        break;
    case QMediaRecorder::RecordingState:
        record();
        break;
    }
    stateChanged(s);
}

void QGstreamerMediaEncoder::record()
{
    if (!m_session)
        return;
    auto oldState = state();
    stateChanged(QMediaRecorder::RecordingState);

    if (oldState == QMediaRecorder::PausedState) {
        // coming from paused state
        stateChanged(QMediaRecorder::RecordingState);
        gstEncoder.setState(GST_STATE_PLAYING);
        updateStatus();
        return;
    }

    updateStatus();
    // create new encoder
    QString outputLocation = m_requestedOutputLocation.toLocalFile();
    if (m_requestedOutputLocation.isEmpty()) {
        QString container = m_resolvedSettings.mimeType().preferredSuffix();
        outputLocation = generateFileName(defaultDir(), container);
    }
    QUrl actualSink = QUrl::fromLocalFile(QDir::currentPath()).resolved(outputLocation);
    qCDebug(qLcMediaEncoder) << "recording new video to" << actualSink;

    Q_ASSERT(!actualSink.isEmpty());
    gstFileSink.set("location", QFile::encodeName(actualSink.toLocalFile()).constData());

    //auto state = gstPipeline.state();
    gstPipeline.setStateSync(GST_STATE_PAUSED);
    gstEncoder.lockState(false);
    gstFileSink.lockState(false);

    audioSrcPad = m_session->getAudioPad();
    if (!audioSrcPad.isNull()) {
        QGstPad audioPad = gstEncoder.getRequestPad("audio_%u");
        audioSrcPad.link(audioPad);
    }

    if (m_resolvedSettings.videoCodec() != QMediaFormat::VideoCodec::Unspecified) {
        videoSrcPad = m_session->getVideoPad();
        if (!videoSrcPad.isNull()) {
            QGstPad videoPad = gstEncoder.getRequestPad("video_%u");
            videoSrcPad.link(videoPad);
        }
    }

    gstEncoder.setState(GST_STATE_PAUSED);
    gstFileSink.setState(GST_STATE_PAUSED);
    gstPipeline.setState(GST_STATE_PLAYING);

    m_duration.start();
    heartbeat.start();
    gstPipeline.dumpGraph("recording");

    actualLocationChanged(QUrl::fromLocalFile(outputLocation));
    updateStatus();
}

void QGstreamerMediaEncoder::pause()
{
    if (!m_session)
        return;
    heartbeat.stop();
    gstPipeline.dumpGraph("before-pause");
    gstEncoder.setState(GST_STATE_PAUSED);

    stateChanged(QMediaRecorder::PausedState);
    updateStatus();
}

void QGstreamerMediaEncoder::stop()
{
    if (!m_session)
        return;
    qCDebug(qLcMediaEncoder) << "stop";
    heartbeat.stop();

    gstPipeline.setStateSync(GST_STATE_PAUSED);
    audioSrcPad.unlinkPeer();
    videoSrcPad.unlinkPeer();
    m_session->releaseAudioPad(audioSrcPad);
    m_session->releaseVideoPad(videoSrcPad);
    audioSrcPad = videoSrcPad = {};

    gstPipeline.setState(GST_STATE_PLAYING);
    //with live sources it's necessary to send EOS even to pipeline
    //before going to STOPPED state
    qCDebug(qLcMediaEncoder) << ">>>>>>>>>>>>> sending EOS";

    gstEncoder.sendEos();
    stateChanged(QMediaRecorder::StoppedState);
    updateStatus();
}

void QGstreamerMediaEncoder::finalize()
{
    if (!m_session)
        return;
    qCDebug(qLcMediaEncoder) << "finalize";

    // The filesink can only be used once, replace it with a new one
    gstPipeline.setStateSync(GST_STATE_PAUSED);
    gstEncoder.lockState(true);
    gstFileSink.lockState(true);
    gstEncoder.setState(GST_STATE_NULL);
    gstFileSink.setState(GST_STATE_NULL);
    gstPipeline.setStateSync(GST_STATE_PLAYING);

    updateStatus();
}

void QGstreamerMediaEncoder::applySettings()
{
    if (!m_session)
        return;
    const auto flag = m_session->camera() ? QMediaFormat::RequiresVideo
                                          : QMediaFormat::NoFlags;
    m_resolvedSettings = m_settings;
    m_resolvedSettings.resolveFormat(flag);

    auto *encodingProfile = createEncodingProfile(m_resolvedSettings);
    g_object_set (gstEncoder.object(), "profile", encodingProfile, nullptr);
    gst_encoding_profile_unref(encodingProfile);
}

void QGstreamerMediaEncoder::setEncoderSettings(const QMediaEncoderSettings &settings)
{
    m_settings = settings;
    applySettings();
}

void QGstreamerMediaEncoder::setMetaData(const QMediaMetaData &metaData)
{
    if (!m_session)
        return;
    m_metaData = static_cast<const QGstreamerMetaData &>(metaData);
    m_metaData.setMetaData(gstEncoder.bin());

}

QMediaMetaData QGstreamerMediaEncoder::metaData() const
{
    return m_metaData;
}

void QGstreamerMediaEncoder::setCaptureSession(QPlatformMediaCaptureSession *session)
{
    QGstreamerMediaCapture *captureSession = static_cast<QGstreamerMediaCapture *>(session);
    if (m_session == captureSession)
        return;

    if (m_session) {
        gstEncoder.setStateSync(GST_STATE_NULL);
        gstFileSink.setStateSync(GST_STATE_NULL);
        gstPipeline.remove(gstEncoder);
        gstPipeline.remove(gstFileSink);
        heartbeat.disconnect();
        QObject::disconnect(cameraChanged);
        gstPipeline.removeMessageFilter(this);
    }

    m_session = captureSession;
    if (!m_session)
        return;

    gstPipeline = captureSession->gstPipeline;
    gstPipeline.set("message-forward", true);
    gstPipeline.installMessageFilter(this);

    // used to update duration every second
    heartbeat.setInterval(1000);
    QObject::connect(&heartbeat, &QTimer::timeout, [this]() { updateDuration(); });

    gstPipeline.add(gstEncoder, gstFileSink);
    gstEncoder.link(gstFileSink);
    gstEncoder.lockState(true);
    gstFileSink.lockState(true); // ### enough with the encoder?

    // ensure we have a usable format
    setEncoderSettings(QMediaEncoderSettings());
    cameraChanged = QObject::connect(m_session, &QGstreamerMediaCapture::cameraChanged, [this]() { applySettings(); });
}

QDir QGstreamerMediaEncoder::defaultDir() const
{
    QStringList dirCandidates;

    if (m_resolvedSettings.videoCodec() != QMediaFormat::VideoCodec::Unspecified)
        dirCandidates << QStandardPaths::writableLocation(QStandardPaths::MoviesLocation);
    else
        dirCandidates << QStandardPaths::writableLocation(QStandardPaths::MusicLocation);

    dirCandidates << QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    dirCandidates << QDir::homePath();
    dirCandidates << QDir::currentPath();
    dirCandidates << QDir::tempPath();

    for (const QString &path : qAsConst(dirCandidates)) {
        QDir dir(path);
        if (dir.exists() && QFileInfo(path).isWritable())
            return dir;
    }

    return QDir();
}

QString QGstreamerMediaEncoder::generateFileName(const QDir &dir, const QString &ext) const
{
    int lastClip = 0;
    const auto list = dir.entryList(QStringList() << QString::fromLatin1("clip_*.%1").arg(ext));
    for (const QString &fileName : list) {
        int imgNumber = QStringView{fileName}.mid(5, fileName.size()-6-ext.length()).toInt();
        lastClip = qMax(lastClip, imgNumber);
    }

    QString name = QString::fromLatin1("clip_%1.%2")
                       .arg(lastClip+1,
                            4, //fieldWidth
                            10,
                            QLatin1Char('0'))
                       .arg(ext);

    return dir.absoluteFilePath(name);
}
