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

#include "qplatformmediaencoder_p.h"

QT_BEGIN_NAMESPACE


/*!
    \class QPlatformMediaEncoder
    \obsolete
    \inmodule QtMultimedia


    \ingroup multimedia_control

    \brief The QPlatformMediaEncoder class provides access to the recording
    functionality.

    This control provides a means to set the \l {outputLocation()}{output location},
    and record, pause and stop recording via the \l setState() method.  It also
    provides feedback on the \l {duration()}{duration} of the recording.

    \sa QMediaRecorder

*/

/*!
    Constructs a media recorder control with the given \a parent.
*/

QPlatformMediaEncoder::QPlatformMediaEncoder(QMediaRecorder *parent)
    : q(parent)
{
}

/*!
    \fn QUrl QPlatformMediaEncoder::outputLocation() const

    Returns the current output location being used.
*/

/*!
    \fn bool QPlatformMediaEncoder::setOutputLocation(const QUrl &location)

    Sets the output \a location and returns if this operation is successful.
    If file at the output location already exists, it should be overwritten.

    The \a location can be relative or empty;
    in this case the service should use the system specific place and file naming scheme.

    After recording has started, the backend should report the actual file location
    with actualLocationChanged() signal.
*/

/*!
    \fn QMediaRecorder::RecorderState QPlatformMediaEncoder::state() const

    Return the current recording state.
*/

/*!
    \fn QMediaRecorder::Status QPlatformMediaEncoder::status() const

    Return the current recording status.
*/

/*!
    \fn qint64 QPlatformMediaEncoder::duration() const

    Return the current duration in milliseconds.
*/

/*!
    \fn void QPlatformMediaEncoder::setState(QMediaRecorder::RecorderState state)

    Set the media recorder \a state.
*/

/*!
    \fn void QPlatformMediaEncoder::applySettings()

    Commits the encoder settings and performs pre-initialization to reduce delays when recording
    is started.
*/

/*!
    \fn bool QPlatformMediaEncoder::isMuted() const

    Returns true if the recorder is muted, and false if it is not.
*/

/*!
    \fn void QPlatformMediaEncoder::stateChanged(QMediaRecorder::RecorderState state)

    Signals that the \a state of a media recorder has changed.
*/
void QPlatformMediaEncoder::stateChanged(QMediaRecorder::RecorderState state)
{
    if (m_state == state)
        return;
    m_state = state;
    emit q->recorderStateChanged(state);
}

/*!
    \fn void QPlatformMediaEncoder::statusChanged(QMediaRecorder::Status status)

    Signals that the \a status of a media recorder has changed.
*/
void QPlatformMediaEncoder::statusChanged(QMediaRecorder::Status status)
{
    if (m_status == status)
        return;
    m_status = status;
    emit q->statusChanged(status);
}

/*!
    \fn void QPlatformMediaEncoder::durationChanged(qint64 duration)

    Signals that the \a duration of the recorded media has changed.

    This only emitted when there is a discontinuous change in the duration such as being reset to 0.
*/
void QPlatformMediaEncoder::durationChanged(qint64 duration)
{
    emit q->durationChanged(duration);
}

/*!
    \fn void QPlatformMediaEncoder::actualLocationChanged(const QUrl &location)

    Signals that the actual media \a location has changed.
    This signal should be emitted at start of recording.
*/
void QPlatformMediaEncoder::actualLocationChanged(const QUrl &location)
{
    if (m_actualLocation == location)
        return;
    m_actualLocation = location;
    emit q->actualLocationChanged(location);
}

/*!
    \fn void QPlatformMediaEncoder::error(int error, const QString &errorString)

    Signals that an \a error has occurred.  The \a errorString describes the error.
*/
void QPlatformMediaEncoder::error(QMediaRecorder::Error error, const QString &errorString)
{
    if (error == m_error && errorString == m_errorString)
        return;
    m_error = error;
    m_errorString = errorString;
    if (error != QMediaRecorder::NoError)
        emit q->errorOccurred(error, errorString);
    emit q->errorChanged();
}

void QPlatformMediaEncoder::metaDataChanged()
{
    emit q->metaDataChanged();
}

QT_END_NAMESPACE
