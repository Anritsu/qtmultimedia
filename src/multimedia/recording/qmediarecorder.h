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

#ifndef QMediaRecorder_H
#define QMediaRecorder_H

#include <QtCore/qobject.h>
#include <QtMultimedia/qtmultimediaglobal.h>
#include <QtMultimedia/qmediaencodersettings.h>
#include <QtMultimedia/qmediaenumdebug.h>
#include <QtMultimedia/qmediametadata.h>

#include <QtCore/qpair.h>

QT_BEGIN_NAMESPACE

class QUrl;
class QSize;
class QAudioFormat;
class QCamera;
class QCameraDevice;
class QAudioDevice;
class QMediaCaptureSession;

class QMediaRecorderPrivate;
class Q_MULTIMEDIA_EXPORT QMediaRecorder : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QMediaRecorder::RecorderState recorderState READ recorderState NOTIFY recorderStateChanged)
    Q_PROPERTY(QMediaRecorder::Status status READ status NOTIFY statusChanged)
    Q_PROPERTY(qint64 duration READ duration NOTIFY durationChanged)
    Q_PROPERTY(QUrl outputLocation READ outputLocation WRITE setOutputLocation)
    Q_PROPERTY(QUrl actualLocation READ actualLocation NOTIFY actualLocationChanged)
    Q_PROPERTY(QMediaMetaData metaData READ metaData WRITE setMetaData NOTIFY metaDataChanged)
    Q_PROPERTY(QMediaRecorder::Error error READ error NOTIFY errorChanged)
    Q_PROPERTY(QString errorString READ errorString NOTIFY errorChanged)
    Q_PROPERTY(QMediaEncoderSettings encoderSettings READ encoderSettings WRITE setEncoderSettings NOTIFY encoderSettingsChanged)

public:
    enum RecorderState
    {
        StoppedState,
        RecordingState,
        PausedState
    };
    Q_ENUM(RecorderState)

    enum Status {
        UnavailableStatus,
        StoppedStatus,
        StartingStatus,
        RecordingStatus,
        PausedStatus,
        FinalizingStatus
    };
    Q_ENUM(Status)

    enum Error
    {
        NoError,
        ResourceError,
        FormatError,
        OutOfSpaceError
    };
    Q_ENUM(Error)

    QMediaRecorder(QObject *parent = nullptr);
    ~QMediaRecorder();

    bool isAvailable() const;

    QUrl outputLocation() const;
    bool setOutputLocation(const QUrl &location);

    QUrl actualLocation() const;

    RecorderState recorderState() const;
    Status status() const;

    Error error() const;
    QString errorString() const;

    qint64 duration() const;

    void setEncoderSettings(const QMediaEncoderSettings &);
    QMediaEncoderSettings encoderSettings() const;

    QMediaMetaData metaData() const;
    void setMetaData(const QMediaMetaData &metaData);
    void addMetaData(const QMediaMetaData &metaData);

    QMediaCaptureSession *captureSession() const;

public Q_SLOTS:
    void record();
    void pause();
    void stop();

Q_SIGNALS:
    void recorderStateChanged(RecorderState state);
    void statusChanged(Status status);
    void durationChanged(qint64 duration);
    void actualLocationChanged(const QUrl &location);
    void encoderSettingsChanged();

    void errorOccurred(Error error, const QString &errorString);
    void errorChanged();

    void metaDataChanged();

private:
    QMediaRecorderPrivate *d_ptr;
    friend class QMediaCaptureSession;
    void setCaptureSession(QMediaCaptureSession *session);
    Q_DISABLE_COPY(QMediaRecorder)
    Q_DECLARE_PRIVATE(QMediaRecorder)
    Q_PRIVATE_SLOT(d_func(), void _q_applySettings())
};

QT_END_NAMESPACE

Q_MEDIA_ENUM_DEBUG(QMediaRecorder, RecorderState)
Q_MEDIA_ENUM_DEBUG(QMediaRecorder, Status)
Q_MEDIA_ENUM_DEBUG(QMediaRecorder, Error)

#endif  // QMediaRecorder_H
