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

#include "Wmcodecdsp.h"
#include "mfaudiodecodercontrol_p.h"

MFAudioDecoderControl::MFAudioDecoderControl(QAudioDecoder *parent)
    : QPlatformAudioDecoder(parent)
    , m_decoderSourceReader(new MFDecoderSourceReader)
    , m_sourceResolver(new SourceResolver)
    , m_resampler(0)
    , m_device(0)
    , m_mfInputStreamID(0)
    , m_mfOutputStreamID(0)
    , m_bufferReady(false)
    , m_duration(-1)
    , m_position(-1)
    , m_loadingSource(false)
    , m_mfOutputType(0)
    , m_convertSample(0)
    , m_sourceReady(false)
    , m_resamplerDirty(false)
{
    CoCreateInstance(CLSID_CResamplerMediaObject, NULL, CLSCTX_INPROC_SERVER, IID_IMFTransform, (LPVOID*)(&m_resampler));
    if (!m_resampler) {
        qCritical("MFAudioDecoderControl: Failed to create resampler(CLSID_CResamplerMediaObject)!");
        return;
    }
    m_resampler->AddInputStreams(1, &m_mfInputStreamID);

    connect(m_sourceResolver, SIGNAL(mediaSourceReady()), this, SLOT(handleMediaSourceReady()));
    connect(m_sourceResolver, SIGNAL(error(long)), this, SLOT(handleMediaSourceError(long)));
    connect(m_decoderSourceReader, SIGNAL(finished()), this, SLOT(handleSourceFinished()));

    QAudioFormat defaultFormat;
    setAudioFormat(defaultFormat);
}

MFAudioDecoderControl::~MFAudioDecoderControl()
{
    if (m_mfOutputType)
        m_mfOutputType->Release();
    m_decoderSourceReader->shutdown();
    m_decoderSourceReader->Release();
    m_sourceResolver->Release();
    if (m_resampler)
        m_resampler->Release();
}

QUrl MFAudioDecoderControl::source() const
{
    return m_source;
}

void MFAudioDecoderControl::onSourceCleared()
{
    bool positionDirty = false;
    bool durationDirty = false;
    if (m_position != -1) {
        m_position = -1;
        positionDirty = true;
    }
    if (m_duration != -1) {
        m_duration = -1;
        durationDirty = true;
    }
    if (positionDirty)
        positionChanged(m_position);
    if (durationDirty)
        durationChanged(m_duration);
}

void MFAudioDecoderControl::setSource(const QUrl &fileName)
{
    if (!m_device && m_source == fileName)
        return;
    m_sourceReady = false;
    m_sourceResolver->cancel();
    m_decoderSourceReader->setSource(nullptr, m_audioFormat);
    m_device = 0;
    m_source = fileName;
    if (!m_source.isEmpty()) {
        m_sourceResolver->shutdown();
        m_sourceResolver->load(m_source, 0);
        m_loadingSource = true;
    } else {
        onSourceCleared();
    }
    sourceChanged();
}

QIODevice* MFAudioDecoderControl::sourceDevice() const
{
    return m_device;
}

void MFAudioDecoderControl::setSourceDevice(QIODevice *device)
{
    if (m_device == device && m_source.isEmpty())
        return;
    m_sourceReady = false;
    m_sourceResolver->cancel();
    m_decoderSourceReader->setSource(nullptr, m_audioFormat);
    m_source.clear();
    m_device = device;
    if (m_device) {
        m_sourceResolver->shutdown();
        m_sourceResolver->load(QUrl(), m_device);
        m_loadingSource = true;
    } else {
        onSourceCleared();
    }
    sourceChanged();
}

void MFAudioDecoderControl::updateResamplerOutputType()
{
    m_resamplerDirty = false;
    HRESULT hr = m_resampler->SetOutputType(m_mfOutputStreamID, m_mfOutputType, 0);
    if (SUCCEEDED(hr)) {
        MFT_OUTPUT_STREAM_INFO streamInfo;
        m_resampler->GetOutputStreamInfo(m_mfOutputStreamID, &streamInfo);
        if ((streamInfo.dwFlags & (MFT_OUTPUT_STREAM_PROVIDES_SAMPLES |  MFT_OUTPUT_STREAM_CAN_PROVIDE_SAMPLES)) == 0) {
            //if resampler does not allocate output sample memory, we do it here
            if (m_convertSample) {
                m_convertSample->Release();
                m_convertSample = 0;
            }
            if (SUCCEEDED(MFCreateSample(&m_convertSample))) {
                IMFMediaBuffer *mbuf = 0;;
                if (SUCCEEDED(MFCreateMemoryBuffer(streamInfo.cbSize, &mbuf))) {
                    m_convertSample->AddBuffer(mbuf);
                    mbuf->Release();
                }
            }
        }
    } else {
        qWarning() << "MFAudioDecoderControl: failed to SetOutputType of resampler" << hr;
    }
}

void MFAudioDecoderControl::handleMediaSourceReady()
{
    m_loadingSource = false;
    m_sourceReady = true;
    IMFMediaType *mediaType = m_decoderSourceReader->setSource(m_sourceResolver->mediaSource(), m_audioFormat);
    m_sourceOutputFormat = QAudioFormat();

    if (mediaType) {
        m_sourceOutputFormat = m_audioFormat;

        UINT32 val = 0;
        if (SUCCEEDED(mediaType->GetUINT32(MF_MT_AUDIO_NUM_CHANNELS, &val))) {
            m_sourceOutputFormat.setChannelCount(int(val));
        }
        if (SUCCEEDED(mediaType->GetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, &val))) {
            m_sourceOutputFormat.setSampleRate(int(val));
        }
        UINT32 bitsPerSample = 0;
        mediaType->GetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, &bitsPerSample);

        GUID subType;
        if (SUCCEEDED(mediaType->GetGUID(MF_MT_SUBTYPE, &subType))) {
            if (subType == MFAudioFormat_Float) {
                m_sourceOutputFormat.setSampleFormat(QAudioFormat::Float);
            } else if (bitsPerSample == 8) {
                m_sourceOutputFormat.setSampleFormat(QAudioFormat::UInt8);
            } else if (bitsPerSample == 16) {
                m_sourceOutputFormat.setSampleFormat(QAudioFormat::Int16);
            } else if (bitsPerSample == 32){
                m_sourceOutputFormat.setSampleFormat(QAudioFormat::Int32);
            }
        }

        if (!m_audioFormat.isValid())
            setResamplerOutputFormat(m_sourceOutputFormat);
        else {
            setResamplerOutputFormat(m_audioFormat);
        }
    }

    if (m_sourceResolver->mediaSource()) {
        if (mediaType && m_resampler) {
            HRESULT hr = S_OK;
            hr = m_resampler->SetInputType(m_mfInputStreamID, mediaType, 0);
            if (SUCCEEDED(hr)) {
                updateResamplerOutputType();
            } else {
                qWarning() << "MFAudioDecoderControl: failed to SetInputType of resampler" << hr;
            }
        }
        IMFPresentationDescriptor *pd;
        if (SUCCEEDED(m_sourceResolver->mediaSource()->CreatePresentationDescriptor(&pd))) {
            UINT64 duration = 0;
            pd->GetUINT64(MF_PD_DURATION, &duration);
            pd->Release();
            duration /= 10000;
            if (m_duration != qint64(duration)) {
                m_duration = qint64(duration);
                durationChanged(m_duration);
            }
        }
        if (isDecoding()) {
            activatePipeline();
        }
    } else if (isDecoding()) {
        setIsDecoding(false);
    }
}

void MFAudioDecoderControl::setResamplerOutputFormat(const QAudioFormat &format)
{
    if (format.isValid()) {
        IMFMediaType *mediaType = 0;
        MFCreateMediaType(&mediaType);
        mediaType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
        if (format.sampleFormat() == QAudioFormat::Float) {
            mediaType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_Float);
        } else {
            mediaType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
        }

        mediaType->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, UINT32(format.channelCount()));
        mediaType->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, UINT32(format.sampleRate()));
        UINT32 alignmentBlock = UINT32(format.bytesPerFrame());
        mediaType->SetUINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, alignmentBlock);
        UINT32 avgBytesPerSec = UINT32(format.sampleRate() * format.bytesPerFrame());
        mediaType->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, avgBytesPerSec);
        mediaType->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, UINT32(format.bytesPerSample()*8));
        mediaType->SetUINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, TRUE);

        if (m_mfOutputType)
            m_mfOutputType->Release();
        m_mfOutputType = mediaType;
    } else {
        if (m_mfOutputType)
            m_mfOutputType->Release();
        m_mfOutputType = NULL;
    }

    if (m_sourceReady && !isDecoding()) {
        updateResamplerOutputType();
    } else {
        m_resamplerDirty = true;
    }
}

void MFAudioDecoderControl::handleMediaSourceError(long hr)
{
    Q_UNUSED(hr);
    m_loadingSource = false;
    m_decoderSourceReader->setSource(nullptr, m_audioFormat);
    setIsDecoding(false);
}

void MFAudioDecoderControl::activatePipeline()
{
    Q_ASSERT(!m_bufferReady);
    setIsDecoding(true);
    connect(m_decoderSourceReader, SIGNAL(sampleAdded()), this, SLOT(handleSampleAdded()));
    if (m_resamplerDirty) {
        updateResamplerOutputType();
    }
    m_decoderSourceReader->reset();
    m_decoderSourceReader->readNextSample();
    if (m_position != -1) {
        m_position = -1;
        positionChanged(-1);
    }
}

void MFAudioDecoderControl::start()
{
    if (isDecoding())
        return;

    if (m_loadingSource) {
        //deferred starting
        setIsDecoding(true);
        return;
    }

    if (!m_decoderSourceReader->mediaSource())
        return;
    activatePipeline();
}

void MFAudioDecoderControl::stop()
{
    if (!isDecoding())
        return;
    disconnect(m_decoderSourceReader, SIGNAL(sampleAdded()), this, SLOT(handleSampleAdded()));
    if (m_bufferReady) {
        m_bufferReady = false;
        emit bufferAvailableChanged(m_bufferReady);
    }
    setIsDecoding(false);
}

void MFAudioDecoderControl::handleSampleAdded()
{
    QList<IMFSample*> samples = m_decoderSourceReader->takeSamples();
    Q_ASSERT(samples.count() > 0);
    Q_ASSERT(!m_bufferReady);
    Q_ASSERT(m_resampler);
    LONGLONG sampleStartTime = 0;
    IMFSample *firstSample = samples.first();
    firstSample->GetSampleTime(&sampleStartTime);
    QByteArray abuf;
    QAudioFormat bufferFormat;
    if (!m_audioFormat.isValid()) {
        bufferFormat = m_sourceOutputFormat;
        //no need for resampling
         for (IMFSample *s : qAsConst(samples)) {
            IMFMediaBuffer *buffer;
            s->ConvertToContiguousBuffer(&buffer);
            DWORD bufLen = 0;
            BYTE *buf = 0;
            if (SUCCEEDED(buffer->Lock(&buf, NULL, &bufLen))) {
                abuf.push_back(QByteArray(reinterpret_cast<char*>(buf), bufLen));
                buffer->Unlock();
            }
            buffer->Release();
            LONGLONG sampleTime = 0, sampleDuration = 0;
            s->GetSampleTime(&sampleTime);
            s->GetSampleDuration(&sampleDuration);
            m_position = qint64(sampleTime + sampleDuration) / 10000;
            s->Release();
        }
    } else {
        bufferFormat = m_audioFormat;
        for (IMFSample *s : qAsConst(samples)) {
            HRESULT hr = m_resampler->ProcessInput(m_mfInputStreamID, s, 0);
            if (SUCCEEDED(hr)) {
                MFT_OUTPUT_DATA_BUFFER outputDataBuffer;
                outputDataBuffer.dwStreamID = m_mfOutputStreamID;
                while (true) {
                    outputDataBuffer.pEvents = 0;
                    outputDataBuffer.dwStatus = 0;
                    outputDataBuffer.pSample = m_convertSample;
                    DWORD status = 0;
                    if (SUCCEEDED(m_resampler->ProcessOutput(0, 1, &outputDataBuffer, &status))) {
                        IMFMediaBuffer *buffer;
                        outputDataBuffer.pSample->ConvertToContiguousBuffer(&buffer);
                        DWORD bufLen = 0;
                        BYTE *buf = 0;
                        if (SUCCEEDED(buffer->Lock(&buf, NULL, &bufLen))) {
                            abuf.push_back(QByteArray(reinterpret_cast<char*>(buf), bufLen));
                            buffer->Unlock();
                        }
                        buffer->Release();
                    } else {
                        break;
                    }
                }
            }
            LONGLONG sampleTime = 0, sampleDuration = 0;
            s->GetSampleTime(&sampleTime);
            s->GetSampleDuration(&sampleDuration);
            m_position = qint64(sampleTime + sampleDuration) / 10000;
            s->Release();
        }
    }
    // WMF uses 100-nanosecond units, QAudioDecoder uses milliseconds, QAudioBuffer uses microseconds...
    m_cachedAudioBuffer = QAudioBuffer(abuf, bufferFormat, qint64(sampleStartTime / 10));
    m_bufferReady = true;
    emit positionChanged(m_position);
    emit bufferAvailableChanged(m_bufferReady);
    emit bufferReady();
}

void MFAudioDecoderControl::handleSourceFinished()
{
    stop();
    emit finished();
}

QAudioFormat MFAudioDecoderControl::audioFormat() const
{
    return m_audioFormat;
}

void MFAudioDecoderControl::setAudioFormat(const QAudioFormat &format)
{
    if (m_audioFormat == format || !m_resampler)
        return;
    m_audioFormat = format;
    setResamplerOutputFormat(format);
    emit formatChanged(m_audioFormat);
}

QAudioBuffer MFAudioDecoderControl::read()
{
    if (!m_bufferReady)
        return QAudioBuffer();
    QAudioBuffer buffer = m_cachedAudioBuffer;
    m_bufferReady = false;
    emit bufferAvailableChanged(m_bufferReady);
    m_decoderSourceReader->readNextSample();
    return buffer;
}

bool MFAudioDecoderControl::bufferAvailable() const
{
    return m_bufferReady;
}

qint64 MFAudioDecoderControl::position() const
{
    return m_position;
}

qint64 MFAudioDecoderControl::duration() const
{
    return m_duration;
}
