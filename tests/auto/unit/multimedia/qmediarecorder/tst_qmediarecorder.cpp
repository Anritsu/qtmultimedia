/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the test suite of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

//TESTED_COMPONENT=src/multimedia

#include <QtTest/QtTest>
#include <QDebug>
#include <QtMultimedia/qmediametadata.h>
#include <private/qplatformmediaencoder_p.h>
#include <qmediarecorder.h>
#include <qaudioformat.h>
#include <qmockintegration_p.h>
#include <qmediacapturesession.h>

#include "qmockmediacapturesession.h"
#include "qmockmediaencoder.h"

QT_USE_NAMESPACE

class tst_QMediaRecorder : public QObject
{
    Q_OBJECT

public slots:
    void initTestCase();
    void cleanupTestCase();

private slots:
    void testBasicSession();
    void testNullControls();
    void testDeleteMediaSource();
    void testError();
    void testSink();
    void testRecord();
    void testEncodingSettings();
    void testAudioSettings();
    void testVideoSettings();
    void testSettingsApplied();

    void metaData();

    void testAudioSettingsCopyConstructor();
    void testAudioSettingsOperatorNotEqual();
    void testAudioSettingsOperatorEqual();
    void testAudioSettingsOperatorAssign();

    void testIsAvailable();
    void testEnum();

    void testVideoSettingsQuality();
    void testVideoSettingsEncodingMode();
    void testVideoSettingsCopyConstructor();
    void testVideoSettingsOperatorAssignment();
    void testVideoSettingsOperatorNotEqual();
    void testVideoSettingsOperatorComparison();

private:
    QMockIntegration *mockIntegration = nullptr;
    QMediaCaptureSession *captureSession;
    QCamera *object = nullptr;
    QMockMediaCaptureSession *service = nullptr;
    QMockMediaEncoder *mock;
    QMediaRecorder *encoder;
};

void tst_QMediaRecorder::initTestCase()
{
    mockIntegration = new QMockIntegration;
    captureSession = new QMediaCaptureSession;
    object = new QCamera;
    encoder = new QMediaRecorder;
    captureSession->setCamera(object);
    captureSession->setEncoder(encoder);
    service = mockIntegration->lastCaptureService();
    mock = service->mockControl;
}

void tst_QMediaRecorder::cleanupTestCase()
{
    delete encoder;
    delete object;
    delete captureSession;
    delete mockIntegration;
}

void tst_QMediaRecorder::testBasicSession()
{
    QMediaCaptureSession session;
    QCamera camera;
    QMediaRecorder recorder;
    session.setCamera(&camera);
    session.setEncoder(&recorder);

    QCOMPARE(recorder.outputLocation(), QUrl());
    QCOMPARE(recorder.recorderState(), QMediaRecorder::StoppedState);
    QCOMPARE(recorder.error(), QMediaRecorder::NoError);
    QCOMPARE(recorder.duration(), qint64(0));
}

void tst_QMediaRecorder::testNullControls()
{
    // With the new changes, hasControls does not make much sense anymore
    // since the session does not own the controls
    // The equivalent of this test would be to not set the control to the session
    // ???
    QMediaCaptureSession session;
    // mockIntegration->lastCaptureService()->hasControls = false;
    QCamera camera;
    QMediaRecorder recorder;
    session.setCamera(&camera);
    // session.setEncoder(&recorder);

    QCOMPARE(recorder.outputLocation(), QUrl());
    QCOMPARE(recorder.recorderState(), QMediaRecorder::StoppedState);
    QCOMPARE(recorder.error(), QMediaRecorder::NoError);
    QCOMPARE(recorder.duration(), qint64(0));

    recorder.setOutputLocation(QUrl("file://test/save/file.mp4"));
    QCOMPARE(recorder.outputLocation(), QUrl());
    QCOMPARE(recorder.actualLocation(), QUrl());

    QMediaEncoderSettings settings;
    settings.setFileFormat(QMediaFormat::MPEG4);
    settings.setAudioCodec(QMediaFormat::AudioCodec::AAC);
    settings.setQuality(QMediaEncoderSettings::LowQuality);

    settings.setVideoCodec(QMediaFormat::VideoCodec::VP9);
    settings.setVideoResolution(640, 480);

    recorder.setEncoderSettings(settings);

    QCOMPARE(recorder.encoderSettings().audioCodec(), QMediaFormat::AudioCodec::AAC);
    QCOMPARE(recorder.encoderSettings().videoCodec(), QMediaFormat::VideoCodec::VP9);
    QCOMPARE(recorder.encoderSettings().fileFormat(), QMediaFormat::MPEG4);

    QSignalSpy spy(&recorder, SIGNAL(recorderStateChanged(RecorderState)));

    recorder.record();
    QCOMPARE(recorder.recorderState(), QMediaRecorder::StoppedState);
    QCOMPARE(recorder.error(), QMediaRecorder::NoError);
    QCOMPARE(spy.count(), 0);

    recorder.pause();
    QCOMPARE(recorder.recorderState(), QMediaRecorder::StoppedState);
    QCOMPARE(recorder.error(), QMediaRecorder::NoError);
    QCOMPARE(spy.count(), 0);

    recorder.stop();
    QCOMPARE(recorder.recorderState(), QMediaRecorder::StoppedState);
    QCOMPARE(recorder.error(), QMediaRecorder::NoError);
    QCOMPARE(spy.count(), 0);
}

void tst_QMediaRecorder::testDeleteMediaSource()
{
    QMediaCaptureSession session;
    QCamera *camera = new QCamera;
    QMediaRecorder *recorder = new QMediaRecorder;
    session.setCamera(camera);
    session.setEncoder(recorder);

    QVERIFY(session.camera() == camera);
    QVERIFY(recorder->isAvailable());

    delete camera;

    QVERIFY(session.camera() == nullptr);
    QVERIFY(recorder->isAvailable());

    delete recorder;
}

void tst_QMediaRecorder::testError()
{
    const QString errorString(QLatin1String("format error"));

    QSignalSpy spy(encoder, SIGNAL(errorOccurred(Error, const QString&)));

    QCOMPARE(encoder->error(), QMediaRecorder::NoError);
    QCOMPARE(encoder->errorString(), QString());

    mock->error(QMediaRecorder::FormatError, errorString);
    QCOMPARE(encoder->error(), QMediaRecorder::FormatError);
    QCOMPARE(encoder->errorString(), errorString);
    QCOMPARE(spy.count(), 1);

    QCOMPARE(spy.last()[0].value<QMediaRecorder::Error>(), QMediaRecorder::FormatError);
}

void tst_QMediaRecorder::testSink()
{
    encoder->setOutputLocation(QUrl("test.tmp"));
    QUrl s = encoder->outputLocation();
    QCOMPARE(s.toString(), QString("test.tmp"));
    QCOMPARE(encoder->actualLocation(), QUrl());

    //the actual location is available after record
    encoder->record();
    QCOMPARE(encoder->actualLocation().toString(), QString("test.tmp"));
    encoder->stop();
    QCOMPARE(encoder->actualLocation().toString(), QString("test.tmp"));

    //setOutputLocation resets the actual location
    encoder->setOutputLocation(QUrl());
    QCOMPARE(encoder->actualLocation(), QUrl());

    encoder->record();
    QCOMPARE(encoder->actualLocation(), QUrl::fromLocalFile("default_name.mp4"));
    encoder->stop();
    QCOMPARE(encoder->actualLocation(), QUrl::fromLocalFile("default_name.mp4"));
}

void tst_QMediaRecorder::testRecord()
{
    QSignalSpy stateSignal(encoder,SIGNAL(recorderStateChanged(RecorderState)));
    QSignalSpy statusSignal(encoder,SIGNAL(statusChanged(Status)));
    QSignalSpy progressSignal(encoder, SIGNAL(durationChanged(qint64)));
    encoder->record();
    QCOMPARE(encoder->recorderState(), QMediaRecorder::RecordingState);
    QCOMPARE(encoder->error(), QMediaRecorder::NoError);
    QCOMPARE(encoder->errorString(), QString());

    QCOMPARE(stateSignal.count(), 1);
    QCOMPARE(stateSignal.last()[0].value<QMediaRecorder::RecorderState>(), QMediaRecorder::RecordingState);

    QTestEventLoop::instance().enterLoop(1);

    QCOMPARE(encoder->status(), QMediaRecorder::RecordingStatus);
    QVERIFY(!statusSignal.isEmpty());
    QCOMPARE(statusSignal.last()[0].value<QMediaRecorder::Status>(), QMediaRecorder::RecordingStatus);
    statusSignal.clear();

    QVERIFY(progressSignal.count() > 0);

    encoder->pause();

    QCOMPARE(encoder->recorderState(), QMediaRecorder::PausedState);

    QCOMPARE(stateSignal.count(), 2);

    QTestEventLoop::instance().enterLoop(1);
    QCOMPARE(encoder->status(), QMediaRecorder::PausedStatus);
    QVERIFY(!statusSignal.isEmpty());
    QCOMPARE(statusSignal.last()[0].value<QMediaRecorder::Status>(), QMediaRecorder::PausedStatus);
    statusSignal.clear();

    encoder->stop();

    QCOMPARE(encoder->recorderState(), QMediaRecorder::StoppedState);
    QCOMPARE(stateSignal.count(), 3);

    QTestEventLoop::instance().enterLoop(1);
    QCOMPARE(encoder->status(), QMediaRecorder::StoppedStatus);
    QVERIFY(!statusSignal.isEmpty());
    QCOMPARE(statusSignal.last()[0].value<QMediaRecorder::Status>(), QMediaRecorder::StoppedStatus);
    statusSignal.clear();

    mock->stop();
    QCOMPARE(stateSignal.count(), 3);
}

void tst_QMediaRecorder::testEncodingSettings()
{
    QMediaEncoderSettings settings = encoder->encoderSettings();
    QCOMPARE(settings.fileFormat(), QMediaFormat::UnspecifiedFormat);
    QCOMPARE(settings.audioCodec(), QMediaFormat::AudioCodec::Unspecified);
    QCOMPARE(settings.videoCodec(), QMediaFormat::VideoCodec::Unspecified);
    QCOMPARE(settings.quality(), QMediaEncoderSettings::NormalQuality);
    QCOMPARE(settings.encodingMode(), QMediaEncoderSettings::ConstantQualityEncoding);

    settings.setAudioCodec(QMediaFormat::AudioCodec::MP3);
    settings.setAudioSampleRate(44100);
    settings.setAudioBitRate(256*1024);
    settings.setQuality(QMediaEncoderSettings::HighQuality);
    settings.setEncodingMode(QMediaEncoderSettings::AverageBitRateEncoding);

    settings.setVideoCodec(QMediaFormat::VideoCodec::H264);
    settings.setVideoBitRate(800);
    settings.setVideoFrameRate(24*1024);
    settings.setVideoResolution(QSize(800,600));

    encoder->setEncoderSettings(settings);

    QCOMPARE(encoder->encoderSettings(), settings);
}

void tst_QMediaRecorder::testAudioSettings()
{
    QMediaEncoderSettings settings;
    QVERIFY(settings == QMediaEncoderSettings());

    QCOMPARE(settings.fileFormat(), QMediaFormat::UnspecifiedFormat);
    QCOMPARE(settings.audioBitRate(), -1);
    QCOMPARE(settings.quality(), QMediaEncoderSettings::NormalQuality);
    QCOMPARE(settings.audioSampleRate(), -1);

    settings.setFileFormat(QMediaFormat::AAC);
    QCOMPARE(settings.fileFormat(), QMediaFormat::AAC);
    QVERIFY(settings != QMediaEncoderSettings());

    settings = QMediaEncoderSettings();
    QCOMPARE(settings.audioBitRate(), -1);
    settings.setAudioBitRate(128000);
    QCOMPARE(settings.audioBitRate(), 128000);

    settings = QMediaEncoderSettings();
    QCOMPARE(settings.quality(), QMediaEncoderSettings::NormalQuality);
    settings.setQuality(QMediaEncoderSettings::HighQuality);
    QCOMPARE(settings.quality(), QMediaEncoderSettings::HighQuality);

    settings = QMediaEncoderSettings();
    QCOMPARE(settings.audioSampleRate(), -1);
    settings.setAudioSampleRate(44100);
    QCOMPARE(settings.audioSampleRate(), 44100);

    settings = QMediaEncoderSettings();
    QCOMPARE(settings.audioChannelCount(), -1);
    settings.setAudioChannelCount(2);
    QCOMPARE(settings.audioChannelCount(), 2);

    settings = QMediaEncoderSettings();

    {
        QMediaEncoderSettings settings1;
        QMediaEncoderSettings settings2;
        QCOMPARE(settings2, settings1);

        settings2 = settings1;
        QCOMPARE(settings2, settings1);

        settings1.setQuality(QMediaEncoderSettings::HighQuality);

        QVERIFY(settings1 != settings2);
    }

    {
        QMediaEncoderSettings settings1;
        QMediaEncoderSettings settings2(settings1);
        QCOMPARE(settings2, settings1);

        settings2 = settings1;
        QCOMPARE(settings2, settings1);

        settings1.setQuality(QMediaEncoderSettings::HighQuality);

        QVERIFY(settings1 != settings2);
    }

    QMediaEncoderSettings settings1;
    settings1.setAudioBitRate(1);
    QMediaEncoderSettings settings2;
    settings2.setAudioBitRate(1);
    QVERIFY(settings1 == settings2);
    settings2.setAudioBitRate(2);
    QVERIFY(settings1 != settings2);

    settings1 = QMediaEncoderSettings();
    settings1.setAudioChannelCount(1);
    settings2 = QMediaEncoderSettings();
    settings2.setAudioChannelCount(1);
    QVERIFY(settings1 == settings2);
    settings2.setAudioChannelCount(2);
    QVERIFY(settings1 != settings2);

    settings1 = QMediaEncoderSettings();
    settings1.setAudioCodec(QMediaFormat::AudioCodec::MP3);
    settings2 = QMediaEncoderSettings();
    settings2.setAudioCodec(QMediaFormat::AudioCodec::MP3);
    QVERIFY(settings1 == settings2);
    settings2.setAudioCodec(QMediaFormat::AudioCodec::FLAC);
    QVERIFY(settings1 != settings2);

    settings1 = QMediaEncoderSettings();
    settings1.setEncodingMode(QMediaEncoderSettings::ConstantBitRateEncoding);
    settings2 = QMediaEncoderSettings();
    settings2.setEncodingMode(QMediaEncoderSettings::ConstantBitRateEncoding);
    QVERIFY(settings1 == settings2);
    settings2.setEncodingMode(QMediaEncoderSettings::AverageBitRateEncoding);
    QVERIFY(settings1 != settings2);

    settings1 = QMediaEncoderSettings();
    settings1.setQuality(QMediaEncoderSettings::NormalQuality);
    settings2 = QMediaEncoderSettings();
    settings2.setQuality(QMediaEncoderSettings::NormalQuality);
    QVERIFY(settings1 == settings2);
    settings2.setQuality(QMediaEncoderSettings::LowQuality);
    QVERIFY(settings1 != settings2);

    settings1 = QMediaEncoderSettings();
    settings1.setAudioSampleRate(1);
    settings2 = QMediaEncoderSettings();
    settings2.setAudioSampleRate(1);
    QVERIFY(settings1 == settings2);
    settings2.setAudioSampleRate(2);
    QVERIFY(settings1 != settings2);
}

void tst_QMediaRecorder::testVideoSettings()
{
    QMediaEncoderSettings settings;
    QVERIFY(settings == QMediaEncoderSettings());

    QCOMPARE(settings.videoCodec(), QMediaFormat::VideoCodec::Unspecified);
    settings.setVideoCodec(QMediaFormat::VideoCodec::H265);
    QCOMPARE(settings.videoCodec(), QMediaFormat::VideoCodec::H265);
    QVERIFY(settings != QMediaEncoderSettings());

    settings = QMediaEncoderSettings();
    QCOMPARE(settings.videoBitRate(), -1);
    settings.setVideoBitRate(128000);
    QCOMPARE(settings.videoBitRate(), 128000);

    settings = QMediaEncoderSettings();
    QCOMPARE(settings.quality(), QMediaEncoderSettings::NormalQuality);
    settings.setQuality(QMediaEncoderSettings::HighQuality);
    QCOMPARE(settings.quality(), QMediaEncoderSettings::HighQuality);

    settings = QMediaEncoderSettings();
    QCOMPARE(settings.videoFrameRate(), -1);
    settings.setVideoFrameRate(60);
    QVERIFY(qFuzzyCompare(settings.videoFrameRate(), qreal(60)));
    settings.setVideoFrameRate(24.0);
    QVERIFY(qFuzzyCompare(settings.videoFrameRate(), qreal(24.0)));

    settings = QMediaEncoderSettings();
    QCOMPARE(settings.videoResolution(), QSize());
    settings.setVideoResolution(QSize(320,240));
    QCOMPARE(settings.videoResolution(), QSize(320,240));
    settings.setVideoResolution(800,600);
    QCOMPARE(settings.videoResolution(), QSize(800,600));

    settings = QMediaEncoderSettings();
    QCOMPARE(settings.videoCodec(), QMediaFormat::VideoCodec::Unspecified);
    QCOMPARE(settings.videoBitRate(), -1);
    QCOMPARE(settings.quality(), QMediaEncoderSettings::NormalQuality);
    QCOMPARE(settings.videoFrameRate(), -1);
    QCOMPARE(settings.videoResolution(), QSize());

    {
        QMediaEncoderSettings settings1;
        QMediaEncoderSettings settings2;
        QCOMPARE(settings2, settings1);

        settings2 = settings1;
        QCOMPARE(settings2, settings1);

        settings1.setQuality(QMediaEncoderSettings::HighQuality);
        QVERIFY(settings1 != settings2);
    }

    {
        QMediaEncoderSettings settings1;
        QMediaEncoderSettings settings2(settings1);
        QCOMPARE(settings2, settings1);

        settings2 = settings1;
        QCOMPARE(settings2, settings1);

        settings1.setQuality(QMediaEncoderSettings::HighQuality);
        QVERIFY(settings1 != settings2);
    }

    QMediaEncoderSettings settings1;
    settings1.setVideoBitRate(1);
    QMediaEncoderSettings settings2;
    settings2.setVideoBitRate(1);
    QVERIFY(settings1 == settings2);
    settings2.setVideoBitRate(2);
    QVERIFY(settings1 != settings2);

    settings1 = QMediaEncoderSettings();
    settings1.setVideoResolution(800,600);
    settings2 = QMediaEncoderSettings();
    settings2.setVideoResolution(QSize(800,600));
    QVERIFY(settings1 == settings2);
    settings2.setVideoResolution(QSize(400,300));
    QVERIFY(settings1 != settings2);

    settings1 = QMediaEncoderSettings();
    settings1.setVideoCodec(QMediaFormat::VideoCodec::H265);
    settings2 = QMediaEncoderSettings();
    settings2.setVideoCodec(QMediaFormat::VideoCodec::H265);
    QVERIFY(settings1 == settings2);
    settings2.setVideoCodec(QMediaFormat::VideoCodec::AV1);
    QVERIFY(settings1 != settings2);

    settings1 = QMediaEncoderSettings();
    settings1.setEncodingMode(QMediaEncoderSettings::ConstantBitRateEncoding);
    settings2 = QMediaEncoderSettings();
    settings2.setEncodingMode(QMediaEncoderSettings::ConstantBitRateEncoding);
    QVERIFY(settings1 == settings2);
    settings2.setEncodingMode(QMediaEncoderSettings::AverageBitRateEncoding);
    QVERIFY(settings1 != settings2);

    settings1 = QMediaEncoderSettings();
    settings1.setQuality(QMediaEncoderSettings::NormalQuality);
    settings2 = QMediaEncoderSettings();
    settings2.setQuality(QMediaEncoderSettings::NormalQuality);
    QVERIFY(settings1 == settings2);
    settings2.setQuality(QMediaEncoderSettings::LowQuality);
    QVERIFY(settings1 != settings2);

    settings1 = QMediaEncoderSettings();
    settings1.setVideoFrameRate(1);
    settings2 = QMediaEncoderSettings();
    settings2.setVideoFrameRate(1);
    QVERIFY(settings1 == settings2);
    settings2.setVideoFrameRate(2);
    QVERIFY(settings1 != settings2);
}

void tst_QMediaRecorder::testSettingsApplied()
{
    QMediaCaptureSession session;
    QMediaRecorder encoder;
    session.setEncoder(&encoder);
    auto *mock = mockIntegration->lastCaptureService()->mockControl;

    //if the media recorder is not configured after construction
    //the settings are applied in the next event loop
    QCOMPARE(mock->m_settingAppliedCount, 0);
    QTRY_COMPARE(mock->m_settingAppliedCount, 1);

    QMediaEncoderSettings settings;
    settings.setVideoResolution(640,480);
    encoder.setEncoderSettings(settings);

    QCOMPARE(mock->m_settingAppliedCount, 1);
    QTRY_COMPARE(mock->m_settingAppliedCount, 2);

    //encoder settings are applied before recording if changed
    settings.setQuality(QMediaEncoderSettings::VeryHighQuality);
    encoder.setEncoderSettings(settings);

    QCOMPARE(mock->m_settingAppliedCount, 2);
    encoder.record();
    QCOMPARE(mock->m_settingAppliedCount, 3);

    encoder.stop();
}

void tst_QMediaRecorder::metaData()
{
    QMediaCaptureSession session;
    QCamera camera;
    QMediaRecorder recorder;
    session.setCamera(&camera);
    session.setEncoder(&recorder);

    QVERIFY(recorder.metaData().isEmpty());

    QMediaMetaData data;
    data.insert(QMediaMetaData::Author, QString::fromUtf8("John Doe"));
    recorder.setMetaData(data);

    QCOMPARE(recorder.metaData().value(QMediaMetaData::Author).toString(), QString::fromUtf8("John Doe"));
}

void tst_QMediaRecorder::testAudioSettingsCopyConstructor()
{
    /* create an object for AudioEncodersettings */
    QMediaEncoderSettings audiosettings;

    /* setting the desired properties for the AudioEncoder */
    audiosettings.setAudioBitRate(128*1000);
    audiosettings.setAudioChannelCount(4);
    audiosettings.setAudioCodec(QMediaFormat::AudioCodec::ALAC);
    audiosettings.setEncodingMode(QMediaEncoderSettings::ConstantBitRateEncoding);
    audiosettings.setQuality(QMediaEncoderSettings::LowQuality);
    audiosettings.setAudioSampleRate(44100);

    /* Copy constructor */
    QMediaEncoderSettings other(audiosettings);

    /* Verifying whether data is copied properly or not */
    QVERIFY(other.audioBitRate() == audiosettings.audioBitRate());
    QVERIFY(other.audioSampleRate() == audiosettings.audioSampleRate());
    QVERIFY(other.audioChannelCount() == audiosettings.audioChannelCount());
    QCOMPARE(other.audioCodec(), audiosettings.audioCodec());
    QVERIFY(other.encodingMode() == audiosettings.encodingMode());
    QVERIFY(other.quality() == audiosettings.quality());
}

void tst_QMediaRecorder::testAudioSettingsOperatorNotEqual()
{
    /* create an object for AudioEncodersettings */
    QMediaEncoderSettings audiosettings1;
    QMediaEncoderSettings audiosettings2;

    /* setting the desired properties to for the AudioEncoder */
    audiosettings1.setAudioBitRate(128*1000);
    audiosettings1.setAudioChannelCount(4);
    audiosettings1.setAudioCodec(QMediaFormat::AudioCodec::FLAC);
    audiosettings1.setEncodingMode(QMediaEncoderSettings::ConstantBitRateEncoding);
    audiosettings1.setQuality(QMediaEncoderSettings::LowQuality);
    audiosettings1.setAudioSampleRate(44100);

    /* setting the desired properties for the AudioEncoder */
    audiosettings2.setAudioBitRate(128*1000);
    audiosettings2.setAudioChannelCount(4);
    audiosettings2.setAudioCodec(QMediaFormat::AudioCodec::FLAC);
    audiosettings2.setEncodingMode(QMediaEncoderSettings::ConstantBitRateEncoding);
    audiosettings2.setQuality(QMediaEncoderSettings::LowQuality);
    audiosettings2.setAudioSampleRate(44100);

    /* verify the both are equal or not */
    QVERIFY(audiosettings1 == audiosettings2);
    QVERIFY(!(audiosettings1 != audiosettings2));

    /* Modify the settings value for one object */
    audiosettings2.setAudioBitRate(64*1000);
    audiosettings2.setEncodingMode(QMediaEncoderSettings::ConstantQualityEncoding);

    /* verify the not equal opertor */
    QVERIFY(audiosettings1 != audiosettings2);

    QVERIFY(audiosettings2.audioBitRate() != audiosettings1.audioBitRate());
    QVERIFY(audiosettings2.encodingMode() != audiosettings1.encodingMode());
}

void tst_QMediaRecorder::testAudioSettingsOperatorEqual()
{
    /* create an object for AudioEncodersettings */
    QMediaEncoderSettings audiosettings1;

    /* setting the desired properties to for the AudioEncoder */
    audiosettings1.setAudioBitRate(128*1000);
    audiosettings1.setAudioChannelCount(4);
    audiosettings1.setAudioCodec(QMediaFormat::AudioCodec::FLAC);
    audiosettings1.setEncodingMode(QMediaEncoderSettings::ConstantBitRateEncoding);
    audiosettings1.setQuality(QMediaEncoderSettings::LowQuality);
    audiosettings1.setAudioSampleRate(44100);

    QMediaEncoderSettings audiosettings2;

    /* setting the desired properties for the AudioEncoder */
    audiosettings2.setAudioBitRate(128*1000);
    audiosettings2.setAudioChannelCount(4);
    audiosettings2.setAudioCodec(QMediaFormat::AudioCodec::FLAC);
    audiosettings2.setEncodingMode(QMediaEncoderSettings::ConstantBitRateEncoding);
    audiosettings2.setQuality(QMediaEncoderSettings::LowQuality);
    audiosettings2.setAudioSampleRate(44100);

    /* verify both the values are same or not */
    QVERIFY(audiosettings1 == audiosettings2);
    audiosettings2.setAudioChannelCount(2);
    QVERIFY(audiosettings1 != audiosettings2);
}

void tst_QMediaRecorder::testAudioSettingsOperatorAssign()
{

    /* create an object for AudioEncodersettings */
    QMediaEncoderSettings audiosettings1;

    /* setting the desired properties for the AudioEncoder */
    audiosettings1.setAudioBitRate(128*1000);
    audiosettings1.setAudioChannelCount(4);
    audiosettings1.setAudioCodec(QMediaFormat::AudioCodec::FLAC);
    audiosettings1.setEncodingMode(QMediaEncoderSettings::ConstantBitRateEncoding);
    audiosettings1.setQuality(QMediaEncoderSettings::LowQuality);
    audiosettings1.setAudioSampleRate(44100);

    QMediaEncoderSettings audiosettings2;
    audiosettings2 = audiosettings1;
    /* Verifying whether data is copied properly or not */
    QVERIFY(audiosettings2.audioBitRate() == audiosettings1.audioBitRate());
    QVERIFY(audiosettings2.audioSampleRate() == audiosettings1.audioSampleRate());
    QVERIFY(audiosettings2.audioChannelCount() == audiosettings1.audioChannelCount());
    QCOMPARE(audiosettings2.audioCodec(), audiosettings1.audioCodec());
    QVERIFY(audiosettings2.encodingMode() == audiosettings1.encodingMode());
    QVERIFY(audiosettings2.quality() == audiosettings1.quality());
}

void tst_QMediaRecorder::testIsAvailable()
{
    {
        QMediaCaptureSession session;
        QCamera camera;
        QMediaRecorder recorder;
        session.setCamera(&camera);
        session.setEncoder(&recorder);
        QCOMPARE(recorder.isAvailable(), true);
    }
    {
        QMediaRecorder recorder;
        QCOMPARE(recorder.isAvailable(), false);
    }
}

/* enum QMediaRecorder::ResourceError property test. */
void tst_QMediaRecorder::testEnum()
{
    const QString errorString(QLatin1String("resource error"));

    QSignalSpy spy(encoder, SIGNAL(errorOccurred(Error, const QString&)));

    QCOMPARE(encoder->error(), QMediaRecorder::NoError);
    QCOMPARE(encoder->errorString(), QString());

    emit mock->error(QMediaRecorder::ResourceError, errorString);
    QCOMPARE(encoder->error(), QMediaRecorder::ResourceError);
    QCOMPARE(encoder->errorString(), errorString);
    QCOMPARE(spy.count(), 1);

    QCOMPARE(spy.last()[0].value<QMediaRecorder::Error>(), QMediaRecorder::ResourceError);
}

/* Test the QMediaEncoderSettings quality API*/
void tst_QMediaRecorder::testVideoSettingsQuality()
{
    /* Create the instance*/
    QMediaEncoderSettings settings;
    QVERIFY(settings == QMediaEncoderSettings());

    /* Verify the default value is intialised correctly*/
    QCOMPARE(settings.quality(), QMediaEncoderSettings::NormalQuality);

    /* Set all types of Quality parameter and Verify if it is set correctly*/
    settings.setQuality(QMediaEncoderSettings::HighQuality);
    QCOMPARE(settings.quality(), QMediaEncoderSettings::HighQuality);

    settings.setQuality(QMediaEncoderSettings::VeryLowQuality);
    QCOMPARE(settings.quality(), QMediaEncoderSettings::VeryLowQuality);

    settings.setQuality(QMediaEncoderSettings::LowQuality);
    QCOMPARE(settings.quality(), QMediaEncoderSettings::LowQuality);

    settings.setQuality(QMediaEncoderSettings::VeryHighQuality);
    QCOMPARE(settings.quality(), QMediaEncoderSettings::VeryHighQuality);
}

/* Test  QMediaEncoderSettings encodingMode */
void tst_QMediaRecorder::testVideoSettingsEncodingMode()
{
    /* Create the instance*/
    QMediaEncoderSettings settings;
    QVERIFY(settings == QMediaEncoderSettings());

    /* Verify the default values are initialised correctly*/
    QCOMPARE(settings.encodingMode(), QMediaEncoderSettings::ConstantQualityEncoding);

    /* Set each type of encoding mode and Verify if it is set correctly*/
    settings.setEncodingMode(QMediaEncoderSettings::ConstantBitRateEncoding);
    QCOMPARE(settings.encodingMode(),QMediaEncoderSettings::ConstantBitRateEncoding);

    settings.setEncodingMode(QMediaEncoderSettings::AverageBitRateEncoding);
    QCOMPARE(settings.encodingMode(), QMediaEncoderSettings::AverageBitRateEncoding);
}

/* Test QMediaEncoderSettings copy constructor */
void tst_QMediaRecorder::testVideoSettingsCopyConstructor()
{
    /* Create the instance and initialise it*/
    QMediaEncoderSettings settings1;
    settings1.setVideoCodec(QMediaFormat::VideoCodec::Theora);
    settings1.setVideoBitRate(128000);
    settings1.setQuality(QMediaEncoderSettings::HighQuality);
    settings1.setEncodingMode(QMediaEncoderSettings::ConstantBitRateEncoding);
    settings1.setVideoFrameRate(60.);
    settings1.setVideoResolution(QSize(320,240));

    /* Create another instance with instance1 as argument*/
    QMediaEncoderSettings settings2(settings1);

    /* Verify if all the parameters are copied correctly*/
    QCOMPARE(settings2 != settings1, false);
    QCOMPARE(settings2.videoCodec(), QMediaFormat::VideoCodec::Theora);
    QCOMPARE(settings2.videoBitRate(), 128000);
    QCOMPARE(settings2.encodingMode(), QMediaEncoderSettings::ConstantBitRateEncoding);
    QVERIFY(qFuzzyCompare(settings2.videoFrameRate(), qreal(60.)));
    QCOMPARE(settings2.videoResolution(), QSize(320,240));
    QCOMPARE(settings2.quality(), QMediaEncoderSettings::HighQuality);

    /* Verify both the instances are equal*/
    QCOMPARE(settings2, settings1);
}

/* Test QMediaEncoderSettings Overloaded Operator assignment*/
void tst_QMediaRecorder::testVideoSettingsOperatorAssignment()
{
    /* Create two instances.*/
    QMediaEncoderSettings settings1;
    QMediaEncoderSettings settings2;
    QCOMPARE(settings2, settings1);

    /* Initialize all the parameters */
    settings1.setVideoCodec(QMediaFormat::VideoCodec::Theora);
    settings1.setVideoBitRate(128000);
    settings1.setEncodingMode(QMediaEncoderSettings::ConstantBitRateEncoding);
    settings1.setVideoFrameRate(60.);
    settings1.setVideoResolution(QSize(320,240));
    settings1.setQuality(QMediaEncoderSettings::HighQuality);
    /* Assign one object to other*/
    settings2 = settings1;

    /* Verify all the parameters are copied correctly*/
    QCOMPARE(settings2, settings1);
    QCOMPARE(settings2.videoCodec(), QMediaFormat::VideoCodec::Theora);
    QCOMPARE(settings2.videoBitRate(), 128000);
    QCOMPARE(settings2.encodingMode(), QMediaEncoderSettings::ConstantBitRateEncoding);
    QVERIFY(qFuzzyCompare(settings2.videoFrameRate(), qreal(60.)));
    QCOMPARE(settings2.videoResolution(), QSize(320,240));
    QCOMPARE(settings2.quality(), QMediaEncoderSettings::HighQuality);
    QCOMPARE(settings2, settings1);
}

/* Test QMediaEncoderSettings Overloaded OperatorNotEqual*/
void tst_QMediaRecorder::testVideoSettingsOperatorNotEqual()
{
    /* Create the instance and set the bit rate and Verify objects with OperatorNotEqual*/
    QMediaEncoderSettings settings1;
    settings1.setVideoBitRate(1);
    QMediaEncoderSettings settings2;
    settings2.setVideoBitRate(1);
    /* OperatorNotEqual returns false when both objects are equal*/
    QCOMPARE(settings1 != settings2, false);
    settings2.setVideoBitRate(2);
    /* OperatorNotEqual returns true when both objects are not equal*/
    QVERIFY(settings1 != settings2);

    /* Verify Resolution with not equal operator*/
    settings1 = QMediaEncoderSettings();
    settings1.setVideoResolution(800,600);
    settings2 = QMediaEncoderSettings();
    settings2.setVideoResolution(QSize(800,600));
    /* OperatorNotEqual returns false when both objects are equal*/
    QCOMPARE(settings1 != settings2, false);
    settings2.setVideoResolution(QSize(400,300));
    /* OperatorNotEqual returns true when both objects are not equal*/
    QVERIFY(settings1 != settings2);

    /* Verify Codec with not equal operator*/
    settings1 = QMediaEncoderSettings();
    settings1.setVideoCodec(QMediaFormat::VideoCodec::Theora);
    settings2 = QMediaEncoderSettings();
    settings2.setVideoCodec(QMediaFormat::VideoCodec::Theora);
    /* OperatorNotEqual returns false when both objects are equal*/
    QCOMPARE(settings1 != settings2, false);
    settings2.setVideoCodec(QMediaFormat::VideoCodec::AV1);
    /* OperatorNotEqual returns true when both objects are not equal*/
    QVERIFY(settings1 != settings2);

    /* Verify EncodingMode with not equal operator*/
    settings1 = QMediaEncoderSettings();
    settings1.setEncodingMode(QMediaEncoderSettings::ConstantBitRateEncoding);
    settings2 = QMediaEncoderSettings();
    settings2.setEncodingMode(QMediaEncoderSettings::ConstantBitRateEncoding);
    /* OperatorNotEqual returns false when both objects are equal*/
    QCOMPARE(settings1 != settings2, false);
    settings2.setEncodingMode(QMediaEncoderSettings::AverageBitRateEncoding);
    /* OperatorNotEqual returns true when both objects are not equal*/
    QVERIFY(settings1 != settings2);

    /* Verify Quality with not equal operator*/
    settings1 = QMediaEncoderSettings();
    settings1.setQuality(QMediaEncoderSettings::NormalQuality);
    settings2 = QMediaEncoderSettings();
    settings2.setQuality(QMediaEncoderSettings::NormalQuality);
    /* OperatorNotEqual returns false when both objects are equal*/
    QCOMPARE(settings1 != settings2, false);
    settings2.setQuality(QMediaEncoderSettings::LowQuality);
    /* OperatorNotEqual returns true when both objects are not equal*/
    QVERIFY(settings1 != settings2);

    /* Verify FrameRate with not equal operator*/
    settings1 = QMediaEncoderSettings();
    settings1.setVideoFrameRate(1);
    settings2 = QMediaEncoderSettings();
    settings2.setVideoFrameRate(1);
    /* OperatorNotEqual returns false when both objects are equal*/
    QCOMPARE(settings1 != settings2, false);
    settings2.setVideoFrameRate(2);
    /* OperatorNotEqual returns true when both objects are not equal*/
    QVERIFY(settings1 != settings2);
}

/* Test QMediaEncoderSettings Overloaded comparison operator*/
void tst_QMediaRecorder::testVideoSettingsOperatorComparison()
{
    /* Create the instance and set the bit rate and Verify objects with comparison operator*/
    QMediaEncoderSettings settings1;
    settings1.setVideoBitRate(1);
    QMediaEncoderSettings settings2;
    settings2.setVideoBitRate(1);

    /* Comparison operator returns true when both objects are equal*/
    QVERIFY(settings1 == settings2);
    settings2.setVideoBitRate(2);
    /* Comparison operator returns false when both objects are not equal*/
    QCOMPARE(settings1 == settings2, false);

    /* Verify resolution with comparison operator*/
    settings1 = QMediaEncoderSettings();
    settings1.setVideoResolution(800,600);
    settings2 = QMediaEncoderSettings();
    settings2.setVideoResolution(QSize(800,600));
    /* Comparison operator returns true when both objects are equal*/
    QVERIFY(settings1 == settings2);
    settings2.setVideoResolution(QSize(400,300));
    /* Comparison operator returns false when both objects are not equal*/
    QCOMPARE(settings1 == settings2, false);

    /* Verify Codec with comparison operator*/
    settings1 = QMediaEncoderSettings();
    settings1.setVideoCodec(QMediaFormat::VideoCodec::Theora);
    settings2 = QMediaEncoderSettings();
    settings2.setVideoCodec(QMediaFormat::VideoCodec::Theora);
    /* Comparison operator returns true when both objects are equal*/
    QVERIFY(settings1 == settings2);
    settings2.setVideoCodec(QMediaFormat::VideoCodec::AV1);
    /* Comparison operator returns false when both objects are not equal*/
    QCOMPARE(settings1 == settings2, false);

    /* Verify EncodingMode with comparison operator*/
    settings1 = QMediaEncoderSettings();
    settings1.setEncodingMode(QMediaEncoderSettings::ConstantBitRateEncoding);
    settings2 = QMediaEncoderSettings();
    settings2.setEncodingMode(QMediaEncoderSettings::ConstantBitRateEncoding);
    /* Comparison operator returns true when both objects are equal*/
    QVERIFY(settings1 == settings2);
    settings2.setEncodingMode(QMediaEncoderSettings::AverageBitRateEncoding);
    /* Comparison operator returns false when both objects are not equal*/
    QCOMPARE(settings1 == settings2, false);

    /* Verify Quality with comparison operator*/
    settings1 = QMediaEncoderSettings();
    settings1.setQuality(QMediaEncoderSettings::NormalQuality);
    settings2 = QMediaEncoderSettings();
    settings2.setQuality(QMediaEncoderSettings::NormalQuality);
    /* Comparison operator returns true when both objects are equal*/
    QVERIFY(settings1 == settings2);
    settings2.setQuality(QMediaEncoderSettings::LowQuality);
    /* Comparison operator returns false when both objects are not equal*/
    QCOMPARE(settings1 == settings2, false);

    /* Verify FrameRate with comparison operator*/
    settings1 = QMediaEncoderSettings();
    settings1.setVideoFrameRate(1);
    settings2 = QMediaEncoderSettings();
    settings2.setVideoFrameRate(1);
    /* Comparison operator returns true when both objects are equal*/
    QVERIFY(settings1 == settings2);
    settings2.setVideoFrameRate(2);
    /* Comparison operator returns false when both objects are not equal*/
    QCOMPARE(settings1 == settings2, false);
}

QTEST_GUILESS_MAIN(tst_QMediaRecorder)
#include "tst_qmediarecorder.moc"
