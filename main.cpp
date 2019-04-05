//#include <QCoreApplication>

#include <iostream>
#include <chrono>
#include <thread>

extern "C"{
#include <libavcodec/avcodec.h>
}

// https://ffmpeg.org/doxygen/2.0/doc_2examples_2decoding_encoding_8c-example.html

class FfmpegAudioCodec
{
	AVPacket m_packet;
	AVFrame* m_frame;
	AVCodec* m_codec;
	AVCodecContext* m_context;
	AVCodecID m_codec_id;
	enum FfmpegCodecDirection
	{
		FFMPEG_ENCODER,
		FFMPEG_DECODER
	} m_codec_direction;
	bool m_open;

public:

	FfmpegAudioCodec(AVCodecID codec_id, bool is_encoder = true) :
		m_codec_id(codec_id),
		m_codec_direction(is_encoder ? FFMPEG_ENCODER : FFMPEG_DECODER),
		m_open(false),
		m_frame(nullptr)
	{
		m_frame = av_frame_alloc();

		if (is_encoder)
		{
			m_codec = avcodec_find_encoder(codec_id);
			m_context = avcodec_alloc_context3(m_codec);
            m_context->bit_rate = 6300;//64000;
            m_context->sample_fmt = AV_SAMPLE_FMT_S16;
            m_context->sample_rate = 8000;
			m_context->channel_layout = AV_CH_LAYOUT_MONO;
            m_context->channels = av_get_channel_layout_nb_channels(m_context->channel_layout);
            m_context->frame_size = 240;

			m_frame->nb_samples      = m_context->frame_size;
			m_frame->format          = m_context->sample_fmt;
            m_frame->channel_layout  = m_context->channel_layout;
            m_frame->channels        = m_context->channels;
            m_frame->sample_rate     = m_context->sample_rate;

		}
		else
		{
			m_codec = avcodec_find_decoder(codec_id);
			m_context = avcodec_alloc_context3(m_codec);
		}

        av_init_packet(&m_packet);

	}

	~FfmpegAudioCodec()
	{
		Close();

        av_free(m_context);
        av_frame_free(&m_frame);
	}

	bool Open()
	{
        int rc = -1;
        // std::this_thread::sleep_for(std::chrono::milliseconds(1000));
		if (!m_open)
		{
            AVDictionary * options = NULL;
            rc = avcodec_open2(m_context, m_codec, &options);
            av_dict_free(&options);
			m_open = (rc >= 0);
		}

		return m_open;
	}

	bool Close()
	{
        int rc = -1;

		if(m_open)
		{
			rc = avcodec_close(m_context) >= 0;
			m_open = false;
		}

		return (rc >= 0);
	}

	bool IsOpen() const
	{
		return m_open;
	}

    std::int32_t Encoder(std::uint16_t* pcm_data, std::int32_t sample_count, std::uint8_t* output_frame, std::int32_t frame_size)
	{
		std::int32_t rc = -1;

		if (IsOpen() && IsEncoder())
		{
            //m_frame->nb_samples = sample_count;
            int buff_size = av_samples_get_buffer_size(NULL, m_context->channels, m_context->frame_size,
                                                                     m_context->sample_fmt, 0);

            rc = avcodec_fill_audio_frame(m_frame, m_context->channels, m_context->sample_fmt,(const uint8_t*)pcm_data, sample_count * 2, 1);

			if (rc >= 0)
			{
				av_init_packet(&m_packet);
                m_packet.data = output_frame;
				m_packet.size = frame_size;

				int got_picture = 0;

                rc = avcodec_encode_audio2(m_context, &m_packet, m_frame, &got_picture);

				if (rc >= 0)
				{

					if (got_picture == 0)
					{
						rc = 0;
					}
                    else
                    {
                        rc = m_packet.size;
                    }
				}
			}
		}

		return rc;
	}

	std::int32_t Decoder(std::uint8_t* input_frame, std::int32_t frame_size, std::uint16_t* pcm_data, std::int32_t sample_count)
	{
		std::int32_t rc = -1;

		if (IsOpen() && !IsEncoder())
		{
			av_init_packet(&m_packet);

            m_packet.data = input_frame;
			m_packet.size = frame_size;

			int got_picture = 0;

            rc = avcodec_decode_audio4(m_context, m_frame, &got_picture, &m_packet);

			if (rc >= 0)
			{
				if (got_picture != 0)
				{
					rc = std::min(sample_count * 2, m_frame->nb_samples * 2);
                    memcpy(pcm_data, m_frame->data[0], rc);
				}
				else
				{
					rc = 0;
				}
			}
		}


		return rc;
	}

	bool IsEncoder() const
	{
		return (m_codec_direction == FFMPEG_ENCODER);
	}
};

template<typename T>
void PrintArray(T* data, int size, const char *sep = "-")
{
    int i = 0;
    while (i < size)
    {
        if (i != 0)
            std::cout << sep;
        std::cout << std::hex << (int)data[i];
        i++;
    }
    std::cout << std::endl;
}

#define SAMPLE_COUNT 240

void test_frame()
{
    AVFrame *frm = av_frame_alloc();
    frm->channels = 1;
    frm->channel_layout = AV_CH_LAYOUT_MONO;
    frm->nb_samples = 240;
    frm->format = AV_SAMPLE_FMT_S16;

    int buff_size = av_samples_get_buffer_size(NULL, frm->channels, frm->nb_samples,
                                                             AV_SAMPLE_FMT_S16, 0);

    uint8_t* samples = (uint8_t*)av_malloc(buff_size);

    int rc = avcodec_fill_audio_frame(frm, frm->channels, AV_SAMPLE_FMT_S16, samples, buff_size, 0);

    std::cout << "fill audio result = " << rc << std::endl;

}

void test1()
{

    FfmpegAudioCodec encoder(AV_CODEC_ID_G723_1, true);
    FfmpegAudioCodec decoder(AV_CODEC_ID_G723_1, false);

    std::uint16_t pcm_data_input[SAMPLE_COUNT];
    std::uint16_t pcm_data_output[SAMPLE_COUNT];
    std::uint8_t enc_data[SAMPLE_COUNT];

    memset(pcm_data_input, 0, sizeof(pcm_data_input));
    memset(pcm_data_output, 0, sizeof(pcm_data_output));
    memset(enc_data, 0, sizeof(enc_data));

    for(int i = 0; i < SAMPLE_COUNT; i++)
    {
        pcm_data_input[i] = 0x1000 + i * 0x50;
    }

    std::cout << "Source speech of " << SAMPLE_COUNT << " samples: " << std::endl;
    PrintArray(pcm_data_input, SAMPLE_COUNT);

    if (encoder.Open())
    {
       auto rc = encoder.Encoder(pcm_data_input, SAMPLE_COUNT, enc_data, 240);
       if (rc > 0)
       {
           std::cout << "Encode frame of " << rc << " bytes: " << std::endl;
           PrintArray(enc_data, rc);

           if (decoder.Open())
           {

               rc = decoder.Decoder(enc_data, rc, pcm_data_output, SAMPLE_COUNT);

               if (rc > 0)
               {
                   std::cout << "Decode frame of " << rc << " samples: " << std::endl;
                   PrintArray(pcm_data_output, rc);
               }

           }
       }
    }


    std::cout << std::endl;

}

int main(int argc, char *argv[])
{
    avcodec_register_all();

    std::cout << "Test encode G723_1 audio codec" << std::endl;

    test1();
    //test_frame();

	return 0;
}
