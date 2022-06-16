/****************************************************************************
**
** Copyright (C) 2022 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL-NOGPL2$
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
** General Public License version 3 or (at your option) any later version
** approved by the KDE Free Qt Foundation. The licenses are as published by
** the Free Software Foundation and appearing in the file LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/
#ifndef QAMBIENTSOUND_H
#define QAMBIENTSOUND_H

#include <QtSpatialAudio/qtspatialaudioglobal.h>
#include <QtMultimedia/qtmultimediaglobal.h>
#include <QtCore/QUrl>
#include <QtCore/QObject>

QT_BEGIN_NAMESPACE

class QAudioEngine;
class QAmbientSoundPrivate;

class Q_SPATIALAUDIO_EXPORT QAmbientSound : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QUrl source READ source WRITE setSource NOTIFY sourceChanged)
    Q_PROPERTY(float volume READ volume WRITE setVolume NOTIFY volumeChanged)
    Q_PROPERTY(int loops READ loops WRITE setLoops NOTIFY loopsChanged)
    Q_PROPERTY(bool autoPlay READ autoPlay WRITE setAutoPlay NOTIFY autoPlayChanged)

public:
    explicit QAmbientSound(QAudioEngine *engine);
    ~QAmbientSound();

    void setSource(const QUrl &url);
    QUrl source() const;

    enum Loops
    {
        Infinite = -1,
        Once = 1
    };
    Q_ENUM(Loops)

    int loops() const;
    void setLoops(int loops);

    bool autoPlay() const;
    void setAutoPlay(bool autoPlay);

    void setVolume(float volume);
    float volume() const;

    QAudioEngine *engine() const;

Q_SIGNALS:
    void sourceChanged();
    void loopsChanged();
    void autoPlayChanged();
    void volumeChanged();

public Q_SLOTS:
    void play();
    void pause();
    void stop();

private:
    void setEngine(QAudioEngine *engine);
    friend class QAmbientSoundPrivate;
    QAmbientSoundPrivate *d = nullptr;
};

QT_END_NAMESPACE

#endif
