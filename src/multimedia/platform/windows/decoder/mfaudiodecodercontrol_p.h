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

#ifndef MFAUDIODECODERCONTROL_H
#define MFAUDIODECODERCONTROL_H

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

#include "private/qplatformaudiodecoder_p.h"
#include "mfdecodersourcereader_p.h"
#include "private/sourceresolver_p.h"

QT_USE_NAMESPACE

class MFAudioDecoderControl : public QPlatformAudioDecoder
{
    Q_OBJECT
public:
    MFAudioDecoderControl(QAudioDecoder *parent);
    ~MFAudioDecoderControl();

    QUrl source() const;
    void setSource(const QUrl &fileName);

    QIODevice* sourceDevice() const;
    void setSourceDevice(QIODevice *device);

    void start();
    void stop();

    QAudioFormat audioFormat() const override;
    void setAudioFormat(const QAudioFormat &format) override;

    QAudioBuffer read();
    bool bufferAvailable() const;

    qint64 position() const;
    qint64 duration() const;

private Q_SLOTS:
    void handleMediaSourceReady();
    void handleMediaSourceError(long hr);
    void handleSampleAdded();
    void handleSourceFinished();

private:
    void updateResamplerOutputType();
    void activatePipeline();
    void onSourceCleared();
    void setResamplerOutputFormat(const QAudioFormat &format);

    MFDecoderSourceReader  *m_decoderSourceReader;
    SourceResolver         *m_sourceResolver;
    IMFTransform           *m_resampler;
    QUrl                    m_source;
    QIODevice              *m_device;
    QAudioFormat            m_audioFormat;
    DWORD                   m_mfInputStreamID;
    DWORD                   m_mfOutputStreamID;
    bool                    m_bufferReady;
    QAudioBuffer            m_cachedAudioBuffer;
    qint64                  m_duration;
    qint64                  m_position;
    bool                    m_loadingSource;
    IMFMediaType           *m_mfOutputType;
    IMFSample              *m_convertSample;
    QAudioFormat            m_sourceOutputFormat;
    bool                    m_sourceReady;
    bool                    m_resamplerDirty;
};

#endif//MFAUDIODECODERCONTROL_H
