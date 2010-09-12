/***************************************************************************
                          soundsourceoggvorbis.cpp  -  ogg vorbis decoder
                             -------------------
    copyright            : (C) 2003 by Svein Magne Bang
    email                :
***************************************************************************/

/***************************************************************************
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
*                                                                         *
***************************************************************************/

#include "trackinfoobject.h"
#include "soundsourceoggvorbis.h"
#include <QtDebug>
#ifdef __WINDOWS__
#include <io.h>
#include <fcntl.h>
#endif

#ifdef __APPLE__
#ifdef __i386
#define OV_ENDIAN_ARG 0
#else
#define OV_ENDIAN_ARG 1
#endif
#endif

#ifdef __LINUX__
#include <endian.h>
#if __BYTE_ORDER == __LITTLE_ENDIAN
#define OV_ENDIAN_ARG 0
#else
#define OV_ENDIAN_ARG 1
#endif
#else
#define OV_ENDIAN_ARG 0
#endif

/*
   Class for reading Ogg Vorbis
 */

SoundSourceOggVorbis::SoundSourceOggVorbis(QString qFilename)
: SoundSource(qFilename)
{
    filelength = 0;
}

SoundSourceOggVorbis::~SoundSourceOggVorbis()
{
    if (filelength > 0){
        ov_clear(&vf);
    }
}

int SoundSourceOggVorbis::open()
{
    QByteArray qBAFilename = m_qFilename.toUtf8();

#ifdef __WINDOWS__
    if(ov_fopen(qBAFilename.data(), &vf) < 0) {
        qDebug() << "oggvorbis: Input does not appear to be an Ogg bitstream.";
        filelength = 0;
        return ERR;
    }
#else
    FILE *vorbisfile =  fopen(qBAFilename.data(), "r");

    if (!vorbisfile) {
        qDebug() << "oggvorbis: cannot open" << m_qFilename;
        return ERR;
    }

    if(ov_open(vorbisfile, &vf, NULL, 0) < 0) {
        qDebug() << "oggvorbis: Input does not appear to be an Ogg bitstream.";
        filelength = 0;
        return ERR;
    }
#endif

    // lookup the ogg's channels and samplerate
    vorbis_info * vi = ov_info(&vf, -1);

    channels = vi->channels;
    m_iSampleRate = vi->rate;

    if(channels > 2){
        qDebug() << "oggvorbis: No support for more than 2 channels!";
        ov_clear(&vf);
        filelength = 0;
        return ERR;
    }

    // ov_pcm_total returns the total number of frames in the ogg file. The
    // frame is the channel-independent measure of samples. The total samples in
    // the file is channels * ov_pcm_total. rryan 7/2009 I verified this by
    // hand. a 30 second long 48khz mono ogg and a 48khz stereo ogg both report
    // 1440000 for ov_pcm_total.
    ogg_int64_t ret = ov_pcm_total(&vf, -1);

    if (ret >= 0) {
        // We pretend that the file is stereo to the rest of the world.
        filelength = ret * 2;
    }
    else //error
    {
      if (ret == OV_EINVAL) {
          //The file is not seekable. Not sure if any action is needed.
      }
    }

    return OK;

}

/*
   seek to <filepos>
 */

long SoundSourceOggVorbis::seek(long filepos)
{
    // In our speak, filepos is a sample in the file abstraction (i.e. it's
    // stereo no matter what). filepos/2 is the frame we want to seek to.
    Q_ASSERT(filepos%2==0);

    if (ov_seekable(&vf)){
        if(ov_pcm_seek(&vf, filepos/2) != 0) {
            // This is totally common (i.e. you're at EOF). Let's not leave this
            // qDebug on.

            // qDebug() << "ogg vorbis: Seek ERR on seekable.";
        }

        // Even if an error occured, return them the current position because
        // that's what we promised. (Double it because ov_pcm_tell returns
        // frames and we pretend to the world that everything is stereo)
        return ov_pcm_tell(&vf) * 2;
    } else{
        qDebug() << "ogg vorbis: Seek ERR.";
        return 0;
    }
}


/*
   read <size> samples into <destination>, and return the number of
   samples actually read.
 */

unsigned SoundSourceOggVorbis::read(volatile unsigned long size, const SAMPLE * destination)
{

    Q_ASSERT(size%2==0);

    char *pRead  = (char*) destination;
    SAMPLE *dest   = (SAMPLE*) destination;



    // 'needed' is size of buffer in bytes. 'size' is size in SAMPLEs,
    // which is 2 bytes.  If the stream is mono, we read 'size' bytes,
    // so that half the buffer is full, then below we double each
    // sample on the left and right channel. If the stream is stereo,
    // then ov_read interleaves the samples into the full length of
    // the buffer.

    // ov_read speaks bytes, we speak words.  needed is the bytes to
    // read, not words to read.

    // size is the maximum space in words that we have in
    // destination. For stereo files, read the full buffer (size*2
    // bytes). For mono files, only read half the buffer (size bytes),
    // and we will double the buffer to be in stereo later.
    unsigned int needed = size * channels;

    unsigned int index=0,ret=0;

    // loop until requested number of samples has been retrieved
    while (needed > 0) {
        // read samples into buffer
        ret = ov_read(&vf, pRead+index, needed, OV_ENDIAN_ARG, 2, 1, &current_section);

        if (ret <= 0) {
            // An error or EOF occured, break out and return what we have sofar.
            break;
        }

        index  += ret;
        needed -= ret;
    }

    // As of here, index is the total bytes read. (index/2/channels) is the
    // total frames read.

    // convert into stereo if file is mono
    if (channels == 1) {
        // rryan 2/2009
        // Mini-proof of the below:
        // size = 20, destination is a 20 element array 0-19
        // readNo = 10 (or less, but 10 in this case)
        // i = 10-1 = 9, so dest[9*2] and dest[9*2+1],
        // so the first iteration touches the very ends of destination
        // on the last iteration, dest[0] and dest[1] are assigned to dest[0]
        for(int i=(index/2-1); i>=0; i--) {
            dest[i*2]     = dest[i];
            dest[(i*2)+1] = dest[i];
        }

        // Pretend we read twice as many bytes as we did, since we just repeated
        // each pair of bytes.
        index *= 2;
    }

    // index is the total bytes read, so the words read is index/2
    return index / 2;
}

/*
   Parse the the file to get metadata
 */
int SoundSourceOggVorbis::parseHeader()
{
    QString filename = this->getFilename();
    QByteArray qBAFilename = filename.toUtf8();
    vorbis_comment *comment = NULL;

#ifdef __WINDOWS__
    if (ov_fopen(qBAFilename.data(), &vf) < 0) {
        qDebug() << "oggvorbis::ParseHeader : Input does not appear to be an Ogg bitstream.";
        return ERR;
    }
#else
    FILE * vorbisfile = fopen(qBAFilename.data(), "r");

    if (!vorbisfile) {
        qDebug() << "oggvorbis::ParseHeader : file cannot be opened.\n";
        return ERR;
    }

    if (ov_open(vorbisfile, &vf, NULL, 0) < 0) {
        qDebug() << "oggvorbis::ParseHeader : Input does not appear to be an Ogg bitstream.\n";
		fclose(vorbisfile);	//should be closed if ov_open fails
        return ERR;
    }
#endif

    comment = ov_comment(&vf, -1);
    if (comment == NULL) {
        qDebug() << "oggvorbis: fatal error reading file.";
        ov_clear(&vf);
        return ERR;
    }

    //precache
    const char* title_p = vorbis_comment_query(comment, (char*)"title", 0); //the char* cast is to shut up the compiler; libvorbis should take `const char*` here but I don't expect us to get them to change that -kousu 2009/02
    const char* artist_p = vorbis_comment_query(comment, (char*)"artist", 0);
    const char* bpm_p = vorbis_comment_query(comment, (char*)"TBPM", 0);
    const char* album_p = vorbis_comment_query(comment, (char*)"album", 0);
    const char* year_p = vorbis_comment_query(comment, (char*)"date", 0);
    const char* genre_p = vorbis_comment_query(comment, (char*)"genre", 0);
    const char* track_p = vorbis_comment_query(comment, (char*)"tracknumber", 0);
    const char* comment_p = vorbis_comment_query(comment, (char*)"description", 0);

    if(title_p)
        setTitle(QString::fromUtf8(title_p));
    if(artist_p)
        setArtist(QString::fromUtf8(artist_p));
    if(album_p)
        setAlbum(QString::fromUtf8(album_p));
    if(year_p)
        setYear(QString::fromUtf8(year_p));
    if(genre_p)
        setGenre(QString::fromUtf8(genre_p));
    if(track_p)
        setTrackNumber(QString::fromUtf8(track_p));
    if (comment_p)
        setComment(QString::fromUtf8(comment_p));
    if (bpm_p) {
        float bpm = str2bpm(bpm_p);
        if(bpm > 0.0f) {
            this->setBPM(bpm);
            //Track->setBpmConfirm(true);
        }
    }
    //this->setHeaderParsed(true);

    this->setType("ogg");
    int duration = (int)ov_time_total(&vf, -1);
    if (duration == OV_EINVAL) {
		ov_clear(&vf);	//close on return !
        return ERR;
    }
    this->setDuration(duration);
    this->setBitrate(ov_bitrate(&vf, -1)/1000);

    vorbis_info *vi=ov_info(&vf,-1);
    if (vi)
    {
        this->setSampleRate(vi->rate);
        this->setChannels(vi->channels);
    }
    else
    {
        qDebug() << "oggvorbis::parseHeader : fatal error reading file.";
        ov_clear(&vf);
        return ERR;
    }

    ov_clear(&vf);
    return OK;
}

/*
   Return the length of the file in samples.
 */

inline long unsigned SoundSourceOggVorbis::length()
{
    return filelength;
}

QList<QString> SoundSourceOggVorbis::supportedFileExtensions()
{
    QList<QString> list;
    list.push_back("ogg");
    return list;
}
