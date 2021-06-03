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

#include "qplatformmediadevices_p.h"
#include "qmediadevices.h"
#include "qaudiodeviceinfo.h"
#include "qcamerainfo.h"
#include "qaudiosystem_p.h"

QT_BEGIN_NAMESPACE

QPlatformMediaDevices::QPlatformMediaDevices() = default;

QPlatformMediaDevices::~QPlatformMediaDevices() = default;

QAudioDeviceInfo QPlatformMediaDevices::audioInput(const QByteArray &id) const
{
    const auto inputs = audioInputs();
    for (auto i : inputs) {
        if (i.id() == id)
            return i;
    }
    return {};
}

QAudioDeviceInfo QPlatformMediaDevices::audioOutput(const QByteArray &id) const
{
    const auto outputs = audioOutputs();
    for (auto o : outputs) {
        if (o.id() == id)
            return o;
    }
    return {};
}

QCameraInfo QPlatformMediaDevices::videoInput(const QByteArray &id) const
{
    const auto inputs = videoInputs();
    for (auto i : inputs) {
        if (i.id() == id)
            return i;
    }
    return QCameraInfo();
}

QPlatformAudioSource* QPlatformMediaDevices::audioInputDevice(const QAudioFormat &format, const QAudioDeviceInfo &deviceInfo)
{
    QAudioDeviceInfo info = deviceInfo;
    if (info.isNull())
        info = audioInputs().value(0);

    QPlatformAudioSource* p = createAudioSource(info);
    if (p)
        p->setFormat(format);
    return p;
}

QAbstractAudioOutput* QPlatformMediaDevices::audioOutputDevice(const QAudioFormat &format, const QAudioDeviceInfo &deviceInfo)
{
    QAudioDeviceInfo info = deviceInfo;
    if (info.isNull())
        info = audioOutputs().value(0);

    QAbstractAudioOutput* p = createAudioOutputDevice(info);
    if (p)
        p->setFormat(format);
    return p;
}

void QPlatformMediaDevices::audioInputsChanged() const
{
    for (auto m : m_devices)
        emit m->audioInputsChanged();
}

void QPlatformMediaDevices::audioOutputsChanged() const
{
    for (auto m : m_devices)
        emit m->audioOutputsChanged();
}

void QPlatformMediaDevices::videoInputsChanged() const
{
    for (auto m : m_devices)
        emit m->videoInputsChanged();
}


QT_END_NAMESPACE
