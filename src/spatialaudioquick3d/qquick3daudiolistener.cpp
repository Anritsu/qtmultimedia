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
#include <qquick3daudiolistener_p.h>
#include <qquick3dspatialsound_p.h>
#include <qquick3daudioengine_p.h>

QT_BEGIN_NAMESPACE

/*!
    \qmltype AudioListener
    \inqmlmodule QtQuick3D.SpatialAudio
    \ingroup quick3d_spatialaudio

    \brief defines the position and orientation of the person listening to a sound field
    defined by a AudioEngine.

    A AudioEngine can have exactly one listener, that defines the position and orientation
    of the person listening to the sounds defined by the objects placed within the audio engine.

    In most cases, the AudioListener should simply be a child of the Camera element in QtQuick3D.
    This will ensure that the sound experience is aligned with the visual rendering of the scene.
 */

QQuick3DAudioListener::QQuick3DAudioListener()
{
    m_listener = new QAudioListener(QQuick3DAudioEngine::getEngine());
    connect(this, &QQuick3DNode::scenePositionChanged, this, &QQuick3DAudioListener::updatePosition);
    connect(this, &QQuick3DNode::sceneRotationChanged, this, &QQuick3DAudioListener::updateRotation);
    updatePosition();
    updateRotation();
}

QQuick3DAudioListener::~QQuick3DAudioListener()
{
    delete m_listener;
}

void QQuick3DAudioListener::updatePosition()
{
    m_listener->setPosition(scenePosition());
}

void QQuick3DAudioListener::updateRotation()
{
    m_listener->setRotation(sceneRotation());
}

QT_END_NAMESPACE
