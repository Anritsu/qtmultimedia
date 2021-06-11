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

    void testIsAvailable();
    void testEnum();

    void testVideoSettingsQuality();
    void testVideoSettingsEncodingMode();

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

    QMediaFormat format;
    format.setFileFormat(QMediaFormat::MPEG4);
    format.setAudioCodec(QMediaFormat::AudioCodec::AAC);
    format.setVideoCodec(QMediaFormat::VideoCodec::VP9);
    recorder.setMediaFormat(format);
    recorder.setQuality(QMediaRecorder::LowQuality);
    recorder.setVideoResolution(640, 480);

    QCOMPARE(recorder.mediaFormat().audioCodec(), QMediaFormat::AudioCodec::AAC);
    QCOMPARE(recorder.mediaFormat().videoCodec(), QMediaFormat::VideoCodec::VP9);
    QCOMPARE(recorder.mediaFormat().fileFormat(), QMediaFormat::MPEG4);

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
    QMediaFormat format = encoder->mediaFormat();
    QCOMPARE(format.fileFormat(), QMediaFormat::UnspecifiedFormat);
    QCOMPARE(format.audioCodec(), QMediaFormat::AudioCodec::Unspecified);
    QCOMPARE(format.videoCodec(), QMediaFormat::VideoCodec::Unspecified);
    QCOMPARE(encoder->quality(), QMediaRecorder::NormalQuality);
    QCOMPARE(encoder->encodingMode(), QMediaRecorder::ConstantQualityEncoding);

    format.setAudioCodec(QMediaFormat::AudioCodec::MP3);
    encoder->setAudioSampleRate(44100);
    encoder->setAudioBitRate(256*1024);
    encoder->setQuality(QMediaRecorder::HighQuality);
    encoder->setEncodingMode(QMediaRecorder::AverageBitRateEncoding);

    format.setVideoCodec(QMediaFormat::VideoCodec::H264);
    encoder->setVideoBitRate(800);
    encoder->setVideoFrameRate(24*1024);
    encoder->setVideoResolution(QSize(800,600));
    encoder->setMediaFormat(format);

    QCOMPARE(encoder->mediaFormat().audioCodec(), QMediaFormat::AudioCodec::MP3);
    QCOMPARE(encoder->audioSampleRate(), 44100);
    QCOMPARE(encoder->audioBitRate(), 256*1024);
    QCOMPARE(encoder->quality(), QMediaRecorder::HighQuality);
    QCOMPARE(encoder->encodingMode(), QMediaRecorder::AverageBitRateEncoding);

    QCOMPARE(encoder->mediaFormat().videoCodec(), QMediaFormat::VideoCodec::H264);
    QCOMPARE(encoder->videoBitRate(), 800);
    QCOMPARE(encoder->videoFrameRate(), 24*1024);
    QCOMPARE(encoder->videoResolution(), QSize(800,600));
}

void tst_QMediaRecorder::testAudioSettings()
{
    QMediaRecorder recorder;

    QCOMPARE(recorder.mediaFormat(), QMediaFormat());
    QCOMPARE(recorder.mediaFormat().fileFormat(), QMediaFormat::UnspecifiedFormat);
    QCOMPARE(recorder.audioBitRate(), -1);
    QCOMPARE(recorder.quality(), QMediaRecorder::NormalQuality);
    QCOMPARE(recorder.audioSampleRate(), -1);

    QMediaFormat format;
    format.setFileFormat(QMediaFormat::AAC);
    recorder.setMediaFormat(format);
    QCOMPARE(recorder.mediaFormat(), format);

    recorder.setAudioBitRate(128000);
    QCOMPARE(recorder.audioBitRate(), 128000);

    recorder.setQuality(QMediaRecorder::HighQuality);
    QCOMPARE(recorder.quality(), QMediaRecorder::HighQuality);

    recorder.setAudioSampleRate(44100);
    QCOMPARE(recorder.audioSampleRate(), 44100);

    QCOMPARE(recorder.audioChannelCount(), -1);
    recorder.setAudioChannelCount(2);
    QCOMPARE(recorder.audioChannelCount(), 2);
}

void tst_QMediaRecorder::testVideoSettings()
{
    QMediaRecorder recorder;

    QCOMPARE(recorder.mediaFormat(), QMediaFormat());
    QCOMPARE(recorder.mediaFormat().videoCodec(), QMediaFormat::VideoCodec::Unspecified);
    QMediaFormat format;
    format.setVideoCodec(QMediaFormat::VideoCodec::H265);
    recorder.setMediaFormat(format);
    QCOMPARE(recorder.mediaFormat(), format);
    QCOMPARE(recorder.mediaFormat().videoCodec(), QMediaFormat::VideoCodec::H265);

    QCOMPARE(recorder.videoBitRate(), -1);
    recorder.setVideoBitRate(128000);
    QCOMPARE(recorder.videoBitRate(), 128000);

    QCOMPARE(recorder.quality(), QMediaRecorder::NormalQuality);
    recorder.setQuality(QMediaRecorder::HighQuality);
    QCOMPARE(recorder.quality(), QMediaRecorder::HighQuality);

    QCOMPARE(recorder.videoFrameRate(), -1);
    recorder.setVideoFrameRate(60);
    QVERIFY(qFuzzyCompare(recorder.videoFrameRate(), qreal(60)));
    recorder.setVideoFrameRate(24.0);
    QVERIFY(qFuzzyCompare(recorder.videoFrameRate(), qreal(24.0)));

    QCOMPARE(recorder.videoResolution(), QSize());
    recorder.setVideoResolution(QSize(320,240));
    QCOMPARE(recorder.videoResolution(), QSize(320,240));
    recorder.setVideoResolution(800,600);
    QCOMPARE(recorder.videoResolution(), QSize(800,600));
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

    encoder.setVideoResolution(640,480);

    QCOMPARE(mock->m_settingAppliedCount, 1);
    QTRY_COMPARE(mock->m_settingAppliedCount, 2);

    //encoder settings are applied before recording if changed
    encoder.setQuality(QMediaRecorder::VeryHighQuality);

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

void tst_QMediaRecorder::testVideoSettingsQuality()
{
    QMediaRecorder recorder;
    /* Verify the default value is intialised correctly*/
    QCOMPARE(recorder.quality(), QMediaRecorder::NormalQuality);

    /* Set all types of Quality parameter and Verify if it is set correctly*/
    recorder.setQuality(QMediaRecorder::HighQuality);
    QCOMPARE(recorder.quality(), QMediaRecorder::HighQuality);

    recorder.setQuality(QMediaRecorder::VeryLowQuality);
    QCOMPARE(recorder.quality(), QMediaRecorder::VeryLowQuality);

    recorder.setQuality(QMediaRecorder::LowQuality);
    QCOMPARE(recorder.quality(), QMediaRecorder::LowQuality);

    recorder.setQuality(QMediaRecorder::VeryHighQuality);
    QCOMPARE(recorder.quality(), QMediaRecorder::VeryHighQuality);
}

void tst_QMediaRecorder::testVideoSettingsEncodingMode()
{
    QMediaRecorder recorder;

    /* Verify the default values are initialised correctly*/
    QCOMPARE(recorder.encodingMode(), QMediaRecorder::ConstantQualityEncoding);

    /* Set each type of encoding mode and Verify if it is set correctly*/
    recorder.setEncodingMode(QMediaRecorder::ConstantBitRateEncoding);
    QCOMPARE(recorder.encodingMode(),QMediaRecorder::ConstantBitRateEncoding);

    recorder.setEncodingMode(QMediaRecorder::AverageBitRateEncoding);
    QCOMPARE(recorder.encodingMode(), QMediaRecorder::AverageBitRateEncoding);
}

QTEST_GUILESS_MAIN(tst_QMediaRecorder)
#include "tst_qmediarecorder.moc"
