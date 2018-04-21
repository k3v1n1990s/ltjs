#include "s_oal.h"
#include <cassert>
#include <cmath>
#include <cstdint>
#include <algorithm>
#include <array>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <iterator>
#include <limits>
#include <list>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <utility>
#include <vector>
#include "al.h"
#include "alc.h"
#include "efx.h"
#include "efx-presets.h"
#include "bibendovsky_spul_algorithm.h"
#include "bibendovsky_spul_file_stream.h"
#include "bibendovsky_spul_substream.h"
#include "bibendovsky_spul_memory_stream.h"
#include "bibendovsky_spul_scope_guard.h"
#include "ltjs_audio_decoder.h"
#include "ltjs_audio_utils.h"


struct OalSoundSys::Impl
{
	static constexpr auto min_aux_sends = 1;
	static constexpr auto default_aux_sends = 2;

	static constexpr auto pan_center = 64;

	static constexpr auto default_pitch = 1.0F;


	using String = std::string;
	using ExtensionsStrings = std::vector<String>;

	using Clock = std::chrono::system_clock;
	using ClockTs = Clock::time_point;

	using MtThread = std::thread;
	using MtMutex = std::mutex;
	using MtMutexGuard = std::lock_guard<MtMutex>;
	using MtUniqueLock = std::unique_lock<MtMutex>;
	using MtCondVar = std::condition_variable;

	static constexpr auto max_user_data_count = 8;
	static constexpr auto max_user_data_index = max_user_data_count - 1;
	using UserDataArray = std::array<sint32, max_user_data_count>;

	using OalContextAttributes = std::vector<ALCint>;


	class Vector3d
	{
	public:
		Vector3d()
			:
			items_{}
		{
		}

		Vector3d(
			const float x,
			const float y,
			const float z)
			:
			items_{x, y, z}
		{
		}


		float& operator[](
			const int index)
		{
			return items_[index];
		}

		float operator[](
			const int index) const
		{
			return items_[index];
		}

		bool operator==(
			const Vector3d& that) const
		{
			return items_ == that.items_;
		}


		bool has_nan() const
		{
			return std::any_of(
				items_.cbegin(),
				items_.cend(),
				[](const auto item)
				{
					return std::isnan(item);
				}
			);
		}

		Vector3d to_lhs() const
		{
			return {items_[0], items_[1], -items_[2]};
		}

		const float* get_c_array() const
		{
			return items_.data();
		}


	private:
		using Items = std::array<float, 3>;


		Items items_;
	}; // Vector3d

	class Orientation3d
	{
	public:
		Orientation3d()
			:
			items_{}
		{
		}

		Orientation3d(
			const float at_x,
			const float at_y,
			const float at_z,
			const float up_x,
			const float up_y,
			const float up_z)
			:
			items_{at_x, at_y, at_z, up_x, up_y, up_z}
		{
		}

		Orientation3d(
			const Vector3d& at,
			const Vector3d& up)
			:
			items_{at[0], at[1], at[2], up[0], up[1], up[2]}
		{
		}


		float& operator[](
			const int index)
		{
			return items_[index];
		}

		float operator[](
			const int index) const
		{
			return items_[index];
		}

		bool operator==(
			const Orientation3d& that) const
		{
			return items_ == that.items_;
		}


		Vector3d& get_at()
		{
			return *reinterpret_cast<Vector3d*>(&items_[0]);
		}

		const Vector3d& get_at() const
		{
			return *reinterpret_cast<const Vector3d*>(&items_[0]);
		}

		Vector3d& get_up()
		{
			return *reinterpret_cast<Vector3d*>(&items_[3]);
		}

		const Vector3d& get_up() const
		{
			return *reinterpret_cast<const Vector3d*>(&items_[3]);
		}


		bool has_nan() const
		{
			return std::any_of(
				items_.cbegin(),
				items_.cend(),
				[](const auto item)
				{
					return std::isnan(item);
				}
			);
		}

		Orientation3d to_lhs() const
		{
			return {items_[0], items_[1], -items_[2], items_[3], items_[4], -items_[5]};
		}

		const float* get_c_array() const
		{
			return items_.data();
		}


	private:
		using Items = std::array<float, 6>;

		Items items_;
	}; // Orientation3d


	static const float min_doppler_factor;
	static const float max_doppler_factor;
	static const float default_3d_doppler_factor;

	static const float min_min_distance;
	static const float max_min_distance;
	static const float default_3d_min_distance;

	static const float min_max_distance;
	static const float max_max_distance;
	static const float default_3d_max_distance;

	static const float min_gain;
	static const float max_gain;
	static const float default_gain;

	static const Vector3d default_3d_position;
	static const Vector3d default_3d_velocity;
	static const Vector3d default_3d_direction;
	static const Orientation3d default_3d_orientation;


	class StreamingSource
	{
	public:
		enum class Type
		{
			none,
			panning,
			spatial,
		}; // Type

		enum class SpatialType
		{
			none,
			source,
			listener,
		}; // SpatialType

		enum class StorageType
		{
			none,
			internal_buffer,
			decoder,
		}; // StorageType

		enum class Status
		{
			none,
			stopped,
			playing,
			failed,
		}; // Status

		struct OpenParameters
		{
			bool is_file_;
			const char* file_name_;
			uint32 file_offset_;

			bool is_mapped_file_;
			const void* mapped_storage_ptr;
			ltjs::AudioDecoder* mapped_decoder_;

			bool is_memory_;
			const void* memory_ptr_;
			uint32 memory_size_;
			ul::WaveFormatEx memory_wave_format_;

			sint32 playback_rate_;

			bool oal_has_effect_slot_;
			ALuint oal_effect_slot_;
		}; // OpenParameters


		static constexpr auto mix_size_ms = 20;

		using OalQueueBuffers = std::vector<ALuint>;
		using MixBuffer = std::vector<std::uint8_t>;

		static constexpr auto oal_max_buffer_count = 3;
		static constexpr auto oal_max_sources = 2;
		static constexpr auto oal_max_pans = 2;

		static constexpr auto left_index = 0;
		static constexpr auto right_index = 1;

		using Data = std::vector<std::uint8_t>;
		using OalPans = std::array<float, oal_max_pans>;
		using OalSources = std::array<ALuint, oal_max_sources>;
		using OalBuffers = std::array<ALuint, oal_max_buffer_count>;


		bool is_looping_;
		bool has_loop_block_;
		sint32 loop_begin_;
		sint32 loop_end_;
		sint32 volume_;
		sint32 pan_;
		float pitch_;
		Data data_;
		UserDataArray user_data_array_;

		OalBuffers oal_buffers_;
		ALuint oal_source_;

		ALenum oal_buffer_format_;
		float oal_gain_;
		OalPans oal_pans_;


		bool is_playing_;
		int mix_size_;
		int mix_sample_count_;
		int data_size_;
		int data_offset_;
		ul::FileStream file_stream_;
		ul::Substream file_substream_;
		ltjs::AudioDecoder decoder_;
		MixBuffer mix_mono_buffer_;
		MixBuffer mix_stereo_buffer_;

		OalQueueBuffers oal_queued_buffers_;
		OalQueueBuffers oal_unqueued_buffers_;

		sint32 master_3d_listener_volume_;
		float oal_master_3d_listener_gain_;
		bool is_3d_listener_muted_;

		float doppler_factor_;
		float min_distance_;
		float max_distance_;
		Vector3d position_;
		Vector3d velocity_;
		Vector3d direction_;
		Orientation3d orientation_;


		StreamingSource(
			const Type type,
			const SpatialType spatial_type)
			:
			is_looping_{},
			has_loop_block_{},
			loop_begin_{},
			loop_end_{},
			volume_{},
			pan_{},
			data_{},
			user_data_array_{},
			oal_buffers_{},
			oal_source_{},
			oal_buffer_format_{},
			oal_gain_{},
			oal_pans_{},
			is_playing_{},
			mix_size_{},
			mix_sample_count_{},
			data_size_{},
			data_offset_{},
			file_stream_{},
			file_substream_{},
			decoder_{},
			mix_mono_buffer_{},
			mix_stereo_buffer_{},
			oal_queued_buffers_{},
			oal_unqueued_buffers_{},
			type_{},
			storage_type_{},
			status_{},
			block_align_{},
			sample_rate_{},
			oal_are_buffers_created_{},
			oal_is_source_created_{},
			master_3d_listener_volume_{},
			oal_master_3d_listener_gain_{},
			is_3d_listener_muted_{},
			doppler_factor_{},
			min_distance_{},
			max_distance_{},
			position_{},
			velocity_{},
			direction_{},
			orientation_{}
		{
			switch (type)
			{
			case Type::panning:
				break;

			case Type::spatial:
				break;

			default:
				throw "Invalid type.";
			}

			switch (spatial_type)
			{
			case SpatialType::none:
				break;

			case SpatialType::source:
				break;

			case SpatialType::listener:
				break;

			default:
				throw "Invalid spatial type.";
			}

			type_ = type;
			spatial_type_ = spatial_type;

			create();
		}

		StreamingSource(
			const StreamingSource& that) = delete;

		StreamingSource& operator=(
			const StreamingSource& that) = delete;

		StreamingSource(
			StreamingSource&& that)
			:
			is_looping_{std::move(that.is_looping_)},
			has_loop_block_{std::move(that.has_loop_block_)},
			loop_begin_{std::move(that.loop_begin_)},
			loop_end_{std::move(that.loop_end_)},
			volume_{std::move(that.volume_)},
			pan_{std::move(that.pan_)},
			pitch_{std::move(that.pitch_)},
			data_{std::move(that.data_)},
			user_data_array_{std::move(that.user_data_array_)},
			oal_buffers_{std::move(that.oal_buffers_)},
			oal_source_{std::move(that.oal_source_)},
			oal_buffer_format_{std::move(that.oal_buffer_format_)},
			oal_gain_{std::move(that.oal_gain_)},
			oal_pans_{std::move(that.oal_pans_)},
			is_playing_{std::move(that.is_playing_)},
			mix_size_{std::move(that.mix_size_)},
			mix_sample_count_{std::move(that.mix_sample_count_)},
			data_size_{std::move(that.data_size_)},
			data_offset_{std::move(that.data_offset_)},
			file_stream_{std::move(that.file_stream_)},
			file_substream_{std::move(that.file_substream_)},
			decoder_{std::move(that.decoder_)},
			mix_mono_buffer_{std::move(that.mix_mono_buffer_)},
			mix_stereo_buffer_{std::move(that.mix_stereo_buffer_)},
			oal_queued_buffers_{std::move(that.oal_queued_buffers_)},
			oal_unqueued_buffers_{std::move(that.oal_unqueued_buffers_)},
			type_{std::move(that.type_)},
			storage_type_{std::move(that.storage_type_)},
			status_{std::move(that.status_)},
			channel_count_{std::move(that.channel_count_)},
			bit_depth_{std::move(that.bit_depth_)},
			block_align_{std::move(that.block_align_)},
			sample_rate_{std::move(that.sample_rate_)},
			oal_are_buffers_created_{std::move(that.oal_are_buffers_created_)},
			oal_is_source_created_{std::move(that.oal_is_source_created_)},
			master_3d_listener_volume_{std::move(that.master_3d_listener_volume_)},
			oal_master_3d_listener_gain_{std::move(that.oal_master_3d_listener_gain_)},
			is_3d_listener_muted_{std::move(that.is_3d_listener_muted_)},
			doppler_factor_{std::move(that.doppler_factor_)},
			min_distance_{std::move(that.min_distance_)},
			max_distance_{std::move(that.max_distance_)},
			position_{std::move(that.position_)},
			velocity_{std::move(that.velocity_)},
			direction_{std::move(that.direction_)},
			orientation_{std::move(that.orientation_)}
		{
			that.oal_are_buffers_created_ = false;
			that.oal_is_source_created_ = false;
		}

		~StreamingSource()
		{
			destroy();
		}


		bool is_panning() const
		{
			return type_ == Type::panning;
		}

		bool is_spatial() const
		{
			return type_ == Type::spatial;
		}

		bool is_listener() const
		{
			return spatial_type_ == SpatialType::listener;
		}

		bool is_mono() const
		{
			return channel_count_ == 1;
		}

		bool is_stereo() const
		{
			return channel_count_ == 2;
		}

		int get_channel_count() const
		{
			return channel_count_;
		}

		int get_bit_depth() const
		{
			return bit_depth_;
		}

		int get_block_align() const
		{
			return block_align_;
		}

		int get_dst_sample_rate() const
		{
			return sample_rate_;
		}

		Status get_status()
		{
			switch (status_)
			{
			case Status::failed:
			case Status::stopped:
				return status_;

			default:
				break;
			}

			auto oal_state = ALint{};

			::alGetSourcei(oal_source_, AL_SOURCE_STATE, &oal_state);
			assert(oal_is_succeed());

			auto status = Status::none;

			switch (oal_state)
			{
			case AL_INITIAL:
			case AL_PAUSED:
			case AL_STOPPED:
				status = Status::stopped;
				break;

			case AL_PLAYING:
				status = Status::playing;
				break;

			default:
				status = Status::failed;
				break;
			}

			status_ = status;

			return status;
		}

		bool is_playing()
		{
			return status_ == Status::playing;
		}

		bool is_stopped()
		{
			return get_status() == Status::stopped;
		}

		bool is_failed()
		{
			return status_ == Status::failed;
		}

		void fail()
		{
			is_playing_ = false;
			status_ = Status::failed;

			::alSourceStop(oal_source_);

			oal_clear_error();
		}

		void pause()
		{
			if (is_failed())
			{
				return;
			}

			is_playing_ = false;
		}

		void resume()
		{
			if (is_failed())
			{
				return;
			}

			is_playing_ = true;
		}

		void stop()
		{
			if (is_failed())
			{
				return;
			}

			is_playing_ = false;

			data_offset_ = 0;

			if (is_looping_ && has_loop_block_)
			{
				data_offset_ = loop_begin_;
			}
		}

		void set_volume(
			const sint32 volume)
		{
			if (!is_panning())
			{
				return;
			}

			const auto new_volume = ltjs::AudioUtils::clamp_lt_volume(volume);

			if (new_volume == volume_)
			{
				return;
			}

			volume_ = new_volume;

			if (is_failed())
			{
				return;
			}

			const auto oal_volume = ltjs::AudioUtils::lt_volume_to_gain(new_volume);

			oal_gain_ = oal_volume;
		}

		void set_pan(
			const sint32 pan)
		{
			if (!is_panning())
			{
				return;
			}

			const auto new_pan = ltjs::AudioUtils::clamp_lt_pan(pan);

			if (pan_ == new_pan)
			{
				return;
			}

			pan_ = new_pan;

			if (is_failed() || is_stereo())
			{
				return;
			}

			const auto oal_pan = ltjs::AudioUtils::lt_pan_to_gain(new_pan);

			if (new_pan == ltjs::AudioUtils::lt_pan_center)
			{
				// Left and right channels are at full volume.

				oal_pans_.fill(default_gain);
			}
			else if (new_pan < ltjs::AudioUtils::lt_pan_center)
			{
				// Left channel is at full volume; right channels is attenuated.

				oal_pans_[left_index] = default_gain;
				oal_pans_[right_index] = std::abs(oal_pan);
			}
			else
			{
				// Right channel is at full volume; lrft channels is attenuated.

				oal_pans_[left_index] = std::abs(oal_pan);
				oal_pans_[right_index] = default_gain;
			}
		}

		void reset()
		{
			decoder_.close();
			file_substream_.close();
			file_stream_.close();

			is_playing_ = {};
			mix_size_ = {};
			mix_sample_count_ = {};
			data_size_ = {};
			data_offset_ = {};

			oal_queued_buffers_.clear();
			oal_queued_buffers_.reserve(oal_max_buffer_count);

			oal_unqueued_buffers_.clear();
			oal_unqueued_buffers_.reserve(oal_max_buffer_count);
			oal_unqueued_buffers_.assign(oal_buffers_.cbegin(), oal_buffers_.cend());

			channel_count_ = {};
			bit_depth_ = {};
			block_align_ = {};
			sample_rate_ = {};

			is_looping_ = {};
			has_loop_block_ = {};
			loop_begin_ = {};
			loop_end_ = {};
			volume_ = ltjs::AudioUtils::lt_max_volume;
			pan_ = pan_center;
		}

		void oal_reset_common()
		{
			pitch_ = default_pitch;
			oal_buffer_format_ = AL_NONE;
			oal_gain_ = default_gain;
			oal_pans_.fill(default_gain);

			::alSourceStop(oal_source_);
			::alSourcei(oal_source_, AL_BUFFER, AL_NONE);
			::alSourcef(oal_source_, AL_PITCH, default_pitch);
			::alSourcef(oal_source_, AL_GAIN, default_gain);
		}

		void oal_reset_spatial()
		{
			if (!is_spatial())
			{
				return;
			}

			oal_gain_ = default_gain;
			master_3d_listener_volume_ = ltjs::AudioUtils::lt_max_volume;
			oal_master_3d_listener_gain_ = default_gain;
			is_3d_listener_muted_ = false;

			doppler_factor_ = default_3d_doppler_factor;
			min_distance_ = default_3d_min_distance;
			max_distance_ = default_3d_max_distance;
			position_ = default_3d_position;
			velocity_ = default_3d_velocity;
			direction_ = default_3d_direction;
			orientation_ = default_3d_orientation;

			oal_clear_error();

			if (is_listener())
			{
				const auto oal_position = position_.to_lhs();
				const auto oal_velocity = velocity_.to_lhs();
				const auto oal_orientation = orientation_.to_lhs();

				::alListenerf(AL_GAIN, oal_gain_);
				assert(oal_is_succeed());
				::alDopplerFactor(doppler_factor_);
				assert(oal_is_succeed());
				::alListenerfv(AL_POSITION, oal_position.get_c_array());
				assert(oal_is_succeed());
				::alListenerfv(AL_VELOCITY, oal_velocity.get_c_array());
				assert(oal_is_succeed());
				::alListenerfv(AL_ORIENTATION, oal_orientation.get_c_array());
				assert(oal_is_succeed());
			}
			else
			{
				const auto oal_position = position_.to_lhs();
				const auto oal_velocity = velocity_.to_lhs();
				const auto oal_direction = direction_.to_lhs();

				::alSourcef(oal_source_, AL_GAIN, oal_gain_);
				assert(oal_is_succeed());
				::alSourcei(oal_source_, AL_SOURCE_RELATIVE, AL_FALSE);
				assert(oal_is_succeed());
				::alSourcef(oal_source_, AL_REFERENCE_DISTANCE, min_distance_);
				assert(oal_is_succeed());
				::alSourcef(oal_source_, AL_MAX_DISTANCE, max_distance_);
				assert(oal_is_succeed());
				::alSourcefv(oal_source_, AL_POSITION, oal_position.get_c_array());
				assert(oal_is_succeed());
				::alSourcefv(oal_source_, AL_VELOCITY, oal_velocity.get_c_array());
				assert(oal_is_succeed());
				::alSourcefv(oal_source_, AL_DIRECTION, oal_direction.get_c_array());
				assert(oal_is_succeed());
			}

			if (!oal_is_succeed())
			{
				fail();
			}
		}

		void open_set_wave_format_internal(
			const ul::WaveFormatEx& wave_format)
		{
			channel_count_ = wave_format.channel_count_;
			bit_depth_ = wave_format.bit_depth_;
			block_align_ = wave_format.block_align_;
			sample_rate_ = static_cast<int>(wave_format.sample_rate_);
		}

		bool open_file_internal(
			const OpenParameters& parameters)
		{
			if (!parameters.is_file_ || !parameters.file_name_)
			{
				return false;
			}

			const auto open_file_result = file_stream_.open(parameters.file_name_, ul::Stream::OpenMode::read);

			if (!open_file_result)
			{
				return false;
			}

			const auto open_subfile_result = file_substream_.open(&file_stream_, parameters.file_offset_);

			if (!open_subfile_result)
			{
				return false;
			}

			auto decoder_parameters = ltjs::AudioDecoder::OpenParameters{};
			decoder_parameters.stream_ptr_ = &file_substream_;

			const auto open_decoder_result = decoder_.open(decoder_parameters);

			if (!open_decoder_result)
			{
				return false;
			}

			storage_type_ = StorageType::decoder;
			data_.clear();
			data_size_ = decoder_.get_data_size();

			open_set_wave_format_internal(decoder_.get_wave_format_ex());

			return true;
		}

		bool open_mapped_file_internal(
			const OpenParameters& parameters)
		{
			if (!parameters.is_mapped_file_ || !parameters.mapped_storage_ptr || !parameters.mapped_decoder_)
			{
				return false;
			}

			const auto mapped_file_size = ltjs::AudioUtils::extract_wave_size(parameters.mapped_storage_ptr);

			if (mapped_file_size <= 0)
			{
				return false;
			}

			auto memory_stream = ul::MemoryStream{parameters.mapped_storage_ptr, mapped_file_size, ul::Stream::OpenMode::read};

			if (!memory_stream.is_open())
			{
				return false;
			}

			auto decoder_parameters = ltjs::AudioDecoder::OpenParameters{};
			decoder_parameters.stream_ptr_ = &memory_stream;

			auto& decoder = *parameters.mapped_decoder_;

			if (!decoder.open(decoder_parameters))
			{
				return false;
			}

			const auto max_decoded_size = decoder.get_data_size();

			data_.resize(max_decoded_size);

			const auto decoded_size = decoder.decode(data_.data(), max_decoded_size);

			if (decoded_size <= 0)
			{
				return false;
			}

			storage_type_ = StorageType::internal_buffer;
			data_size_ = decoded_size;

			open_set_wave_format_internal(decoder.get_wave_format_ex());

			return true;
		}

		bool open_memory_internal(
			const OpenParameters& parameters)
		{
			if (!parameters.is_memory_ || !parameters.memory_ptr_ || parameters.memory_size_ <= 0)
			{
				return false;
			}

			if (!validate_wave_format_ex(parameters.memory_wave_format_))
			{
				return false;
			}

			data_.resize(parameters.memory_size_);

			std::uninitialized_copy_n(
				static_cast<const std::uint8_t*>(parameters.memory_ptr_),
				parameters.memory_size_,
				data_.begin());

			storage_type_ = StorageType::internal_buffer;

			data_size_ = static_cast<int>(parameters.memory_size_);

			open_set_wave_format_internal(parameters.memory_wave_format_);

			return true;
		}

		bool open_initialize_internal(
			const OpenParameters& parameters)
		{
			if (is_spatial() && !is_mono())
			{
				return false;
			}

			oal_reset_common();
			oal_reset_spatial();

			const auto has_pitch = (parameters.playback_rate_ > 0 && sample_rate_ != parameters.playback_rate_);

			if (has_pitch)
			{
				pitch_ = static_cast<float>(parameters.playback_rate_) / static_cast<float>(sample_rate_);
			}

			mix_sample_count_ = static_cast<int>((pitch_ * mix_size_ms * sample_rate_) / 1000.0F);
			mix_size_ = mix_sample_count_ * block_align_;

			mix_mono_buffer_.resize(mix_size_);

			if (is_spatial())
			{
				oal_buffer_format_ = get_oal_buffer_format(channel_count_, bit_depth_);
			}
			else
			{
				if (is_mono())
				{
					mix_stereo_buffer_.resize(mix_size_ * 2);
				}

				oal_buffer_format_ = get_oal_buffer_format(2, bit_depth_);
			}

			oal_clear_error();

			::alSourceStop(oal_source_);
			::alSourcei(oal_source_, AL_BUFFER, AL_NONE);
			::alSourcef(oal_source_, AL_PITCH, pitch_);
			::alSourcef(oal_source_, AL_GAIN, default_gain);

			if (is_spatial())
			{
				const auto zero_vector = Vector3d{};

				::alSourcefv(oal_source_, AL_VELOCITY, reinterpret_cast<const ALfloat*>(&zero_vector));
				::alSourcefv(oal_source_, AL_POSITION, reinterpret_cast<const ALfloat*>(&zero_vector));
				::alSourcefv(oal_source_, AL_DIRECTION, reinterpret_cast<const ALfloat*>(&zero_vector));
				::alSourcef(oal_source_, AL_REFERENCE_DISTANCE, ltjs::AudioUtils::ds_default_min_distance);
				::alSourcef(oal_source_, AL_MAX_DISTANCE, ltjs::AudioUtils::ds_default_max_distance);
			}

			::alSourcei(oal_source_, AL_SOURCE_RELATIVE, is_spatial() ? AL_FALSE : AL_TRUE);

			if (parameters.oal_has_effect_slot_)
			{
				::alSource3i(oal_source_, AL_AUXILIARY_SEND_FILTER, parameters.oal_effect_slot_, 0, AL_FILTER_NULL);
			}

			if (!oal_is_succeed())
			{
				status_ = Status::failed;
				return false;
			}

			is_playing_ = false;
			status_ = Status::stopped;

			return true;
		}

		bool open_internal(
			const OpenParameters& parameters)
		{
			reset();

			if (parameters.is_file_)
			{
				if (!open_file_internal(parameters))
				{
					return false;
				}
			}
			else if (parameters.is_mapped_file_)
			{
				if (!open_mapped_file_internal(parameters))
				{
					return false;
				}
			}
			else if (parameters.is_memory_)
			{
				if (!open_memory_internal(parameters))
				{
					return false;
				}
			}
			else
			{
				return false;
			}

			if (!open_initialize_internal(parameters))
			{
				return false;
			}

			return true;
		}

		void close_internal()
		{
			decoder_.close();
			file_substream_.close();
			file_stream_.close();
		}

		bool open(
			const OpenParameters& parameters)
		{
			if (!open_internal(parameters))
			{
				close_internal();
				return false;
			}

			return true;
		}

		void set_loop_block(
			const sint32 loop_begin_offset,
			const sint32 loop_end_offset,
			const bool is_enable)
		{
			const auto data_size = static_cast<int>(data_.size());
			const auto sample_size = block_align_;

			auto new_start = loop_begin_offset;

			if (new_start <= 0)
			{
				new_start = 0;
			}

			new_start /= sample_size;
			new_start *= sample_size;

			auto new_end = loop_end_offset;

			if (new_end < 0)
			{
				new_end = data_size;
			}

			new_end /= sample_size;
			new_end *= sample_size;

			has_loop_block_ = is_enable;
			loop_begin_ = new_start;
			loop_end_ = new_end;

			if ((new_start == 0 && new_end == data_size) || new_start > new_end)
			{
				has_loop_block_ = false;
			}
		}

		void set_loop(
			const bool is_enable)
		{
			if (is_looping_ == is_enable)
			{
				return;
			}

			is_looping_ = is_enable;
		}

		void set_ms_position(
			const sint32 milliseconds)
		{
			if (is_failed())
			{
				return;
			}

			const auto sample_offset = static_cast<int>((sample_rate_ * milliseconds) / 1000LL);
			const auto data_offset = sample_offset * block_align_;
			const auto data_size = static_cast<int>(data_.size());

			if (data_offset > data_size)
			{
				return;
			}

			data_offset_ = data_offset;
		}

		void fill_mix_stereo_buffer_u8(
			const int byte_offset)
		{
			const auto sample_count = byte_offset / block_align_;
			const auto oal_left_volume_scale = oal_gain_ * oal_pans_[0];
			const auto oal_right_volume_scale = oal_gain_ * oal_pans_[1];

			const auto src_buffer = mix_mono_buffer_.data();
			auto dst_buffer = mix_stereo_buffer_.data();

			auto dst_index = 0;

			for (auto i = 0; i < sample_count; ++i)
			{
				const auto oal_gain = ((src_buffer[i] - 128) / 128.0F);
				const auto oal_left_gain = oal_gain * oal_left_volume_scale;
				const auto oal_right_gain = oal_gain * oal_right_volume_scale;

				const auto left_volume_u8 = static_cast<std::uint8_t>(ul::Algorithm::clamp(
					static_cast<int>((oal_left_gain * 128.0F) + 128.5F), 0, 255));

				const auto right_volume_u8 = static_cast<std::uint8_t>(ul::Algorithm::clamp(
					static_cast<int>((oal_right_gain * 128.0F) + 128.5F), 0, 255));

				dst_buffer[dst_index++] = left_volume_u8;
				dst_buffer[dst_index++] = right_volume_u8;
			}

			const auto remain_sample_count = mix_sample_count_ - sample_count;

			if (remain_sample_count > 0)
			{
				std::uninitialized_fill_n(&dst_buffer[dst_index], remain_sample_count * 2, std::uint8_t{});
			}
		}

		void fill_mix_stereo_buffer_i16(
			const int byte_offset)
		{
			const auto sample_count = byte_offset / block_align_;
			const auto oal_left_volume_scale = oal_gain_ * oal_pans_[0];
			const auto oal_right_volume_scale = oal_gain_ * oal_pans_[1];

			const auto src_buffer = reinterpret_cast<const std::int16_t*>(mix_mono_buffer_.data());
			auto dst_buffer = reinterpret_cast<std::int16_t*>(mix_stereo_buffer_.data());

			auto dst_index = 0;

			for (auto i = 0; i < sample_count; ++i)
			{
				const auto oal_gain = src_buffer[i] / 32768.0F;
				const auto oal_left_gain = oal_gain * oal_left_volume_scale;
				const auto oal_right_gain = oal_gain * oal_right_volume_scale;

				const auto left_volume_i16 = static_cast<std::int16_t>(ul::Algorithm::clamp(
					static_cast<int>(oal_left_gain * 32768.0F), -32768, 32767));

				const auto right_volume_i16 = static_cast<std::int16_t>(ul::Algorithm::clamp(
					static_cast<int>(oal_right_gain * 32768.0F), -32768, 32768));

				dst_buffer[dst_index++] = left_volume_i16;
				dst_buffer[dst_index++] = right_volume_i16;
			}

			const auto remain_sample_count = mix_sample_count_ - sample_count;

			if (remain_sample_count > 0)
			{
				std::uninitialized_fill_n(&dst_buffer[dst_index], remain_sample_count * 2, std::int16_t{});
			}
		}

		int fill_mix_buffer()
		{
			const auto is_looping = is_looping_;

			const auto data_begin_offset = static_cast<int>(
				is_looping && has_loop_block_ ? loop_begin_ : 0);

			auto data_end_offset = static_cast<int>(
				is_looping && has_loop_block_ ? loop_end_ : data_size_);

			auto mix_offset = 0;

			while (mix_offset < mix_size_)
			{
				const auto data_remain_size = data_end_offset - data_offset_;

				if (data_remain_size == 0)
				{
					if (is_looping)
					{
						data_offset_ = data_begin_offset;

						const auto sample_offset = data_begin_offset / get_block_align();

						if (storage_type_ == StorageType::decoder)
						{
							if (!decoder_.set_position(sample_offset))
							{
								fail();
								break;
							}
						}

						continue;
					}
					else
					{
						break;
					}
				}

				const auto mix_remain_size = mix_size_ - mix_offset;
				const auto to_decode_size = std::min(data_remain_size, mix_remain_size);

				if (to_decode_size == 0)
				{
					break;
				}

				auto decoded_size = 0;

				if (storage_type_ == StorageType::decoder)
				{
					decoded_size = decoder_.decode(&mix_mono_buffer_[mix_offset], to_decode_size);
				}
				else if (storage_type_ == StorageType::internal_buffer)
				{
					decoded_size = std::min(to_decode_size, data_end_offset - data_offset_);

					std::uninitialized_copy_n(
						data_.cbegin() + data_offset_,
						decoded_size,
						mix_mono_buffer_.begin() + mix_offset);
				}
				else
				{
					fail();
					break;
				}

				if (decoded_size > 0)
				{
					mix_offset += decoded_size;
					data_offset_ += decoded_size;
				}
				else
				{
					if (decoded_size == 0 && storage_type_ == StorageType::decoder)
					{
						// Adjust the actual data size if necessary.
						//
						const auto actual_data_size = decoder_.get_decoded_size();

						if (data_size_ != actual_data_size)
						{
							data_size_ = actual_data_size;

							if (!is_looping)
							{
								data_end_offset = actual_data_size;
							}
						}
					}

					// TODO Use the appropriate value for 8 bit depth.
					std::uninitialized_fill_n(mix_mono_buffer_.begin() + mix_offset, mix_size_ - mix_offset, std::uint8_t{});

					break;
				}
			}

			if (!is_spatial() && is_mono())
			{
				switch (get_bit_depth())
				{
				case 8:
					fill_mix_stereo_buffer_u8(mix_offset);
					break;

				case 16:
					fill_mix_stereo_buffer_i16(mix_offset);
					break;

				default:
					break;
				}
			}

			return mix_offset;
		}

		//
		// Fills a stream with data.
		//
		// Returns:
		//    - "true" - if stream is processed.
		//    - "false" - if stream is not processed (paused, failed, etc).
		//
		bool mix()
		{
			if (is_failed())
			{
				return false;
			}

			const auto is_sample_playing = (status_ == Status::playing);

			if (!is_playing_)
			{
				if (is_sample_playing)
				{
					::alSourcePause(oal_source_);
					assert(oal_is_succeed());
					status_ = Status::stopped;
					return false;
				}
				else
				{
					return false;
				}
			}

			auto oal_processed = ALint{};
			auto oal_queued = ALint{};

			::alGetSourcei(oal_source_, AL_BUFFERS_PROCESSED, &oal_processed);
			assert(oal_is_succeed());
			::alGetSourcei(oal_source_, AL_BUFFERS_QUEUED, &oal_queued);
			assert(oal_is_succeed());

			if (oal_processed > 0)
			{
				auto buffers = OalBuffers{};
				const auto old_size = oal_unqueued_buffers_.size();

				oal_unqueued_buffers_.resize(
					old_size + std::min(oal_processed, oal_max_buffer_count));

				::alSourceUnqueueBuffers(
					oal_source_,
					oal_processed,
					&oal_unqueued_buffers_[old_size]);

				assert(oal_is_succeed());
			}

			if (!is_looping_ && data_offset_ == data_size_)
			{
				if (oal_queued == 0)
				{
					is_playing_ = false;
					::alSourcePause(oal_source_);
					assert(oal_is_succeed());
					status_ = Status::stopped;
					return false;
				}

				if (oal_queued > 0)
				{
					return true;
				}
			}

			auto queued_count = 0;

			for (auto i = oal_queued; i < oal_max_buffer_count; ++i)
			{
				if (oal_unqueued_buffers_.empty())
				{
					break;
				}

				auto decoded_mix_size = fill_mix_buffer();

				if (decoded_mix_size == 0)
				{
					break;
				}

				const auto oal_buffer = oal_unqueued_buffers_.back();
				oal_unqueued_buffers_.pop_back();
				oal_queued_buffers_.push_back(oal_buffer);

				auto buffer_size = mix_size_;
				auto buffer_data = mix_mono_buffer_.data();

				if (!is_spatial() && is_mono())
				{
					buffer_size *= 2;
					buffer_data = mix_stereo_buffer_.data();
				}

				::alBufferData(
					oal_buffer,
					oal_buffer_format_,
					buffer_data,
					buffer_size,
					get_dst_sample_rate());

				assert(oal_is_succeed());

				::alSourceQueueBuffers(oal_source_, 1, &oal_buffer);

				assert(oal_is_succeed());

				queued_count += 1;

				if (decoded_mix_size < mix_size_)
				{
					break;
				}
			}

			if (queued_count > 0)
			{
				const auto oal_status = get_status();

				if (is_failed())
				{
					return false;
				}

				if (oal_status != Status::playing)
				{
					::alSourcePlay(oal_source_);
					assert(oal_is_succeed());
					status_ = Status::playing;
				}
			}

			return oal_processed > 0 || queued_count > 0;
		}

		void set_master_3d_listener_volume(
			const sint32 lt_volume)
		{
			if (!is_spatial() && !is_listener())
			{
				return;
			}

			const auto new_lt_volume = ltjs::AudioUtils::clamp_lt_volume(lt_volume);

			if (new_lt_volume == master_3d_listener_volume_)
			{
				return;
			}

			master_3d_listener_volume_ = new_lt_volume;

			if (is_failed())
			{
				return;
			}

			oal_master_3d_listener_gain_ = ltjs::AudioUtils::lt_volume_to_gain(master_3d_listener_volume_);

			if (is_3d_listener_muted_)
			{
				::alListenerf(AL_GAIN, min_gain);
				assert(oal_is_succeed());
			}
			else
			{
				const auto oal_gain = oal_gain_ * oal_master_3d_listener_gain_;

				::alListenerf(AL_GAIN, oal_gain);
				assert(oal_is_succeed());
			}
		}

		void mute_3d_listener(
			const bool is_muted)
		{
			if (!is_spatial() && !is_listener())
			{
				return;
			}

			if (is_muted == is_3d_listener_muted_)
			{
				return;
			}

			is_3d_listener_muted_ = is_muted;


			if (is_failed())
			{
				return;
			}

			if (is_3d_listener_muted_)
			{
				::alListenerf(AL_GAIN, min_gain);
				assert(oal_is_succeed());
			}
			else
			{
				const auto oal_gain = oal_gain_ * oal_master_3d_listener_gain_;

				::alListenerf(AL_GAIN, oal_gain);
				assert(oal_is_succeed());
			}
		}

		void set_3d_volume(
			const sint32 lt_volume)
		{
			if (!is_spatial())
			{
				return;
			}

			const auto new_lt_volume = ltjs::AudioUtils::clamp_lt_volume(lt_volume);

			if (new_lt_volume == volume_)
			{
				return;
			}

			volume_ = new_lt_volume;

			if (is_failed())
			{
				return;
			}

			oal_gain_ = ltjs::AudioUtils::lt_volume_to_gain(volume_);

			if (is_listener())
			{
				if (is_3d_listener_muted_)
				{
					oal_gain_ = {};
				}
				else
				{
					oal_gain_ *= oal_master_3d_listener_gain_;
				}

				::alListenerf(AL_GAIN, oal_gain_);
				assert(oal_is_succeed());
			}
			else
			{
				::alSourcef(oal_source_, AL_GAIN, oal_gain_);
				assert(oal_is_succeed());
			}
		}

		sint32 get_3d_volume() const
		{
			return volume_;
		}

		void set_3d_doppler_factor(
			const float doppler_factor)
		{
			if (!is_spatial() || !is_listener() || std::isnan(doppler_factor))
			{
				return;
			}

			const auto new_doppler_factor = ul::Algorithm::clamp(
				doppler_factor, min_doppler_factor, max_doppler_factor);

			if (new_doppler_factor == doppler_factor_)
			{
				return;
			}

			doppler_factor_ = new_doppler_factor;

			if (is_failed())
			{
				return;
			}

			::alDopplerFactor(doppler_factor_);
			assert(oal_is_succeed());
		}

		float get_3d_doppler_factor() const
		{
			return doppler_factor_;
		}

		void set_3d_distances(
			const float min_distance,
			const float max_distance)
		{
			if (!is_spatial() || is_listener() || std::isnan(min_distance) || std::isnan(max_distance))
			{
				return;
			}

			if (min_distance >= max_distance)
			{
				return;
			}

			const auto new_min_distance = ul::Algorithm::clamp(min_distance, min_min_distance, min_max_distance);
			const auto new_max_distance = ul::Algorithm::clamp(max_distance, max_min_distance, max_max_distance);

			if (new_min_distance == min_distance_ && new_max_distance == max_distance_)
			{
				return;
			}

			min_distance_ = new_min_distance;
			max_distance_ = new_max_distance;

			if (is_failed())
			{
				return;
			}

			::alSourcef(oal_source_, AL_REFERENCE_DISTANCE, min_distance_);
			assert(oal_is_succeed());
			::alSourcef(oal_source_, AL_MAX_DISTANCE, max_distance_);
			assert(oal_is_succeed());
		}

		float get_3d_min_distance() const
		{
			return min_distance_;
		}

		float get_3d_max_distance() const
		{
			return max_distance_;
		}

		void set_3d_position(
			const Vector3d& position)
		{
			if (!is_spatial() || position.has_nan())
			{
				return;
			}

			if (position == position_)
			{
				return;
			}

			position_ = position;

			if (is_failed())
			{
				return;
			}

			const auto& oal_position = position_.to_lhs();
			const auto oal_position_c_array = oal_position.get_c_array();

			if (is_listener())
			{
				::alListenerfv(AL_POSITION, oal_position_c_array);
				assert(oal_is_succeed());
			}
			else
			{
				::alSourcefv(oal_source_, AL_POSITION, oal_position_c_array);
				assert(oal_is_succeed());
			}
		}

		const Vector3d& get_3d_position() const
		{
			return position_;
		}

		void set_3d_velocity(
			const Vector3d& velocity)
		{
			if (!is_spatial() || velocity.has_nan())
			{
				return;
			}

			if (velocity == velocity_)
			{
				return;
			}

			velocity_ = velocity;

			if (is_failed())
			{
				return;
			}

			const auto& oal_velocity = velocity_.to_lhs();
			const auto oal_velocity_c_array = oal_velocity.get_c_array();

			if (is_listener())
			{
				::alListenerfv(AL_VELOCITY, oal_velocity_c_array);
				assert(oal_is_succeed());
			}
			else
			{
				::alSourcefv(oal_source_, AL_VELOCITY, oal_velocity_c_array);
				assert(oal_is_succeed());
			}
		}

		const Vector3d& get_3d_velocity() const
		{
			return velocity_;
		}

		void set_3d_direction(
			const Vector3d& direction)
		{
			if (!is_spatial() || is_listener() || direction.has_nan())
			{
				return;
			}

			if (direction == direction_)
			{
				return;
			}

			direction_ = direction;

			if (is_failed())
			{
				return;
			}

			const auto& oal_direction = direction_.to_lhs();

			::alSourcefv(oal_source_, AL_DIRECTION, oal_direction.get_c_array());
			assert(oal_is_succeed());
		}

		const Vector3d& get_3d_direction() const
		{
			return direction_;
		}

		void set_3d_orientation(
			const Orientation3d& orientation)
		{
			if (!is_spatial() || !is_listener() || orientation.has_nan())
			{
				return;
			}

			if (orientation == orientation_)
			{
				return;
			}

			orientation_ = orientation;

			if (is_failed())
			{
				return;
			}

			const auto& oal_orientation = orientation_.to_lhs();

			::alListenerfv(AL_ORIENTATION, oal_orientation.get_c_array());
			assert(oal_is_succeed());
		}

		const Orientation3d& get_3d_orientation() const
		{
			return orientation_;
		}


	private:
		Type type_;
		SpatialType spatial_type_;
		StorageType storage_type_;
		Status status_;

		int channel_count_;
		int bit_depth_;
		int block_align_;
		int sample_rate_;

		bool oal_are_buffers_created_;
		bool oal_is_source_created_;


		void create()
		{
			if (is_listener())
			{
				oal_reset_spatial();
				return;
			}

			auto is_succeed = true;

			oal_clear_error();

			::alGenBuffers(oal_max_buffer_count, oal_buffers_.data());

			if (oal_is_succeed())
			{
				oal_are_buffers_created_ = true;
			}
			else
			{
				is_succeed = false;
				assert(!"alGenBuffers");
			}

			::alGenSources(1, &oal_source_);

			if (oal_is_succeed())
			{
				oal_is_source_created_ = true;
			}
			else
			{
				is_succeed = false;
				assert(!"alGenSources");
			}

			if (!is_succeed)
			{
				destroy();

				status_ = Status::failed;

				return;
			}

			status_ = {};
		}

		void destroy()
		{
			if (is_listener())
			{
				return;
			}

			if (oal_is_source_created_)
			{
				oal_is_source_created_ = false;

#ifdef _DEBUG
				oal_clear_error();
#endif // _DEBUG

				::alSourceStop(oal_source_);

#ifdef _DEBUG
				if (!oal_is_succeed())
				{
					assert(!"alSourceStop");
				}
#endif // _DEBUG

				::alDeleteSources(1, &oal_source_);

#ifdef _DEBUG
				if (!oal_is_succeed())
				{
					assert(!"alDeleteSources");
				}
#endif // _DEBUG
			}

			if (oal_are_buffers_created_)
			{
				oal_are_buffers_created_ = false;

#ifdef _DEBUG
				oal_clear_error();
#endif // _DEBUG

				::alDeleteBuffers(oal_max_buffer_count, oal_buffers_.data());

#ifdef _DEBUG
				if (!oal_is_succeed())
				{
					assert(!"alDeleteBuffers");
				}
#endif // _DEBUG
			}

			decoder_.close();
			file_substream_.close();
			file_stream_.close();

			status_ = {};
		}
	}; // StreamingSource


	using StreamingSourceUPtr = std::unique_ptr<StreamingSource>;
	using Sources = std::list<StreamingSource>;
	using OpenSources = std::list<StreamingSource*>;


	// =========================================================================
	// Utils
	// =========================================================================
	//

	void destroy_streams()
	{
		mt_is_stop_sound_worker_ = true;

		mt_notify_stream();

		if (mt_sound_thread_.joinable())
		{
			mt_sound_thread_.join();
		}

		mt_is_stop_sound_worker_ = false;

		streams_.clear();
		mt_open_streams_.clear();
	}

	void create_streams()
	{
		destroy_streams();

		mt_sound_thread_ = MtThread{std::bind(&Impl::sound_worker, this)};
	}

	void mt_notify_stream()
	{
		MtUniqueLock cv_lock{mt_sound_cv_mutex_};
		mt_sound_cv_flag_ = true;
		mt_sound_cv_.notify_one();
	}

	void mt_wait_for_stream_cv()
	{
		MtUniqueLock cv_lock{mt_sound_cv_mutex_};
		mt_sound_cv_.wait(cv_lock, [&](){ return mt_sound_cv_flag_; });
		mt_sound_cv_flag_ = false;
	}

	void sound_worker()
	{
		const auto sleep_delay = std::chrono::milliseconds{10};

		while (!mt_is_stop_sound_worker_)
		{
			auto is_mixed = false;

			auto are_samples_idle = false;

			{
				MtUniqueLock lock{mt_samples_mutex_};

				if (mt_open_samples_.empty())
				{
					are_samples_idle = true;
				}
				else
				{
					const auto total_count = static_cast<int>(mt_open_samples_.size());

					auto paused_count = 0;

					for (auto stream_ptr : mt_open_samples_)
					{
						auto& stream = *stream_ptr;

						is_mixed |= stream.mix();

						if (!stream.is_playing())
						{
							paused_count += 1;
						}
					}

					if (paused_count == total_count)
					{
						are_samples_idle = true;
					}
				}
			}


			auto are_3d_objects_idle = false;

			{
				MtUniqueLock lock{mt_3d_objects_mutex_};

				if (mt_open_3d_objects_.empty())
				{
					are_3d_objects_idle = true;
				}
				else
				{
					const auto total_count = static_cast<int>(mt_open_3d_objects_.size());

					auto paused_count = 0;

					for (auto stream_ptr : mt_open_3d_objects_)
					{
						auto& stream = *stream_ptr;

						is_mixed |= stream.mix();

						if (!stream.is_playing())
						{
							paused_count += 1;
						}
					}

					if (paused_count == total_count)
					{
						are_3d_objects_idle = true;
					}
				}
			}


			auto are_streams_idle = false;

			{
				MtUniqueLock lock{mt_streams_mutex_};

				if (mt_open_streams_.empty())
				{
					are_streams_idle = true;
				}
				else
				{
					const auto total_count = static_cast<int>(mt_open_streams_.size());

					auto paused_count = 0;

					for (auto stream_ptr : mt_open_streams_)
					{
						auto& stream = *stream_ptr;

						is_mixed |= stream.mix();

						if (!stream.is_playing())
						{
							paused_count += 1;
						}
					}

					if (paused_count == total_count)
					{
						are_streams_idle = true;
					}
				}
			}

			if (are_samples_idle && are_3d_objects_idle && are_streams_idle)
			{
				mt_wait_for_stream_cv();
			}
			else if (!is_mixed)
			{
				std::this_thread::sleep_for(sleep_delay);
			}
		}
	}

	using EfxReverbPresets = std::array<EFXEAXREVERBPROPERTIES, ltjs::AudioUtils::eax_environment_count>;

	static const EfxReverbPresets efx_reverb_presets;

	static const EFXEAXREVERBPROPERTIES& get_efx_reverb_preset(
		const int preset_index)
	{
		auto new_preset_index = preset_index;

		if (new_preset_index < 0 || new_preset_index > ltjs::AudioUtils::eax_max_environment)
		{
			new_preset_index = ltjs::AudioUtils::eax_default_environment;
		}

		return efx_reverb_presets[new_preset_index];
	}

	static void lt_reverb_to_efx_reverb(
		const LTFILTERREVERB& lt_reverb,
		EFXEAXREVERBPROPERTIES& efx_reverb)
	{
		// Normalize EAX parameters.
		//
#ifdef USE_EAX20_HARDWARE_FILTERS
		const auto eax_environment = ul::Algorithm::clamp(
			lt_reverb.lEnvironment,
			ltjs::AudioUtils::eax_min_environment,
			ltjs::AudioUtils::eax_max_environment);
#endif // USE_EAX20_HARDWARE_FILTERS

		const auto eax_environment_diffusion = ul::Algorithm::clamp(
			lt_reverb.fDiffusion,
			ltjs::AudioUtils::eax_min_environment_diffusion,
			ltjs::AudioUtils::eax_max_environment_diffusion);

		const auto eax_room = ul::Algorithm::clamp(
			lt_reverb.lRoom,
			ltjs::AudioUtils::eax_min_room,
			ltjs::AudioUtils::eax_max_room);

		const auto eax_room_hf = ul::Algorithm::clamp(
			lt_reverb.lRoomHF,
			ltjs::AudioUtils::eax_min_room_hf,
			ltjs::AudioUtils::eax_max_room_hf);

		const auto eax_decay_time = ul::Algorithm::clamp(
			lt_reverb.fDecayTime,
			ltjs::AudioUtils::eax_min_decay_time,
			ltjs::AudioUtils::eax_max_decay_time);

		const auto eax_decay_hf_ratio = ul::Algorithm::clamp(
			lt_reverb.fDecayHFRatio,
			ltjs::AudioUtils::eax_min_decay_hf_ratio,
			ltjs::AudioUtils::eax_max_decay_hf_ratio);

		const auto eax_reflections = ul::Algorithm::clamp(
			lt_reverb.lReflections,
			ltjs::AudioUtils::eax_min_reflections,
			ltjs::AudioUtils::eax_max_reflections);

		const auto eax_reflections_delay = ul::Algorithm::clamp(
			lt_reverb.fReflectionsDelay,
			ltjs::AudioUtils::eax_min_reflections_delay,
			ltjs::AudioUtils::eax_max_reflections_delay);

		const auto eax_reverb = ul::Algorithm::clamp(
			lt_reverb.lReverb,
			ltjs::AudioUtils::eax_min_reverb,
			ltjs::AudioUtils::eax_max_reverb);

		const auto eax_reverb_delay = ul::Algorithm::clamp(
			lt_reverb.fReverbDelay,
			ltjs::AudioUtils::eax_min_reverb_delay,
			ltjs::AudioUtils::eax_max_reverb_delay);

		const auto eax_room_rolloff_factor = ul::Algorithm::clamp(
			lt_reverb.fRoomRolloffFactor,
			ltjs::AudioUtils::eax_min_room_rolloff_factor,
			ltjs::AudioUtils::eax_max_room_rolloff_factor);

#ifdef USE_EAX20_HARDWARE_FILTERS
		const auto eax_air_absorbtion_hf = ul::Algorithm::clamp(
			lt_reverb.fAirAbsorptionHF,
			ltjs::AudioUtils::eax_min_air_absorption_hf,
			ltjs::AudioUtils::eax_max_air_absorption_hf);
#endif // USE_EAX20_HARDWARE_FILTERS


		// Normalize EFX parameters.
		//
		const auto efx_diffusion = ul::Algorithm::clamp(
			eax_environment_diffusion,
			AL_EAXREVERB_MIN_DIFFUSION,
			AL_EAXREVERB_MAX_DIFFUSION);

		const auto efx_gain = ul::Algorithm::clamp(
			ltjs::AudioUtils::ds_volume_to_gain(eax_room),
			AL_EAXREVERB_MIN_GAIN,
			AL_EAXREVERB_MAX_GAIN);

		const auto efx_gain_hf = ul::Algorithm::clamp(
			ltjs::AudioUtils::ds_volume_to_gain(eax_room_hf),
			AL_EAXREVERB_MIN_GAINHF,
			AL_EAXREVERB_MAX_GAINHF);

		const auto efx_decay_time = ul::Algorithm::clamp(
			eax_decay_time,
			AL_EAXREVERB_MIN_DECAY_TIME,
			AL_EAXREVERB_MAX_DECAY_TIME);

		const auto efx_decay_hf_ratio = ul::Algorithm::clamp(
			eax_decay_hf_ratio,
			AL_EAXREVERB_MIN_DECAY_HFRATIO,
			AL_EAXREVERB_MAX_DECAY_HFRATIO);

		const auto efx_reflections_gain = ul::Algorithm::clamp(
			ltjs::AudioUtils::mb_volume_to_gain(eax_reflections),
			AL_EAXREVERB_MIN_REFLECTIONS_GAIN,
			AL_EAXREVERB_MAX_REFLECTIONS_GAIN);

		const auto efx_reflections_delay = ul::Algorithm::clamp(
			eax_reflections_delay,
			AL_EAXREVERB_MIN_REFLECTIONS_DELAY,
			AL_EAXREVERB_MAX_REFLECTIONS_DELAY);

		const auto efx_late_reverb_gain = ul::Algorithm::clamp(
			ltjs::AudioUtils::mb_volume_to_gain(eax_reverb),
			AL_EAXREVERB_MIN_LATE_REVERB_GAIN,
			AL_EAXREVERB_MAX_LATE_REVERB_GAIN);

		const auto efx_late_reverb_delay = ul::Algorithm::clamp(
			eax_reverb_delay,
			AL_EAXREVERB_MIN_LATE_REVERB_DELAY,
			AL_EAXREVERB_MAX_LATE_REVERB_DELAY);

		const auto efx_room_rolloff_factor = ul::Algorithm::clamp(
			eax_room_rolloff_factor,
			AL_EAXREVERB_MIN_ROOM_ROLLOFF_FACTOR,
			AL_EAXREVERB_MAX_ROOM_ROLLOFF_FACTOR);

#ifdef USE_EAX20_HARDWARE_FILTERS
		const auto efx_air_absorption_gain_hf = ul::Algorithm::clamp(
			ltjs::AudioUtils::mb_volume_to_gain(static_cast<int>(eax_air_absorbtion_hf)),
			AL_EAXREVERB_MIN_AIR_ABSORPTION_GAINHF,
			AL_EAXREVERB_MAX_AIR_ABSORPTION_GAINHF);
#endif // USE_EAX20_HARDWARE_FILTERS


		// Set all properties according to the environmetn value.
		//
#ifdef USE_EAX20_HARDWARE_FILTERS
		efx_reverb = get_efx_reverb_preset(eax_environment);
#else
		efx_reverb = get_efx_reverb_preset(ltjs::AudioUtils::eax_default_environment);
#endif // USE_EAX20_HARDWARE_FILTERS


		// Set specific properties.
		//
		efx_reverb.flDiffusion = efx_diffusion;
		efx_reverb.flGain = efx_gain;
		efx_reverb.flGainHF = efx_gain_hf;
		efx_reverb.flDecayTime = efx_decay_time;
		efx_reverb.flDecayHFRatio = efx_decay_hf_ratio;
		efx_reverb.flReflectionsGain = efx_reflections_gain;
		efx_reverb.flReflectionsDelay = efx_reflections_delay;
		efx_reverb.flLateReverbGain = efx_late_reverb_gain;
		efx_reverb.flLateReverbDelay = efx_late_reverb_delay;
		efx_reverb.flRoomRolloffFactor = efx_room_rolloff_factor;

#ifdef USE_EAX20_HARDWARE_FILTERS
		efx_reverb.flAirAbsorptionGainHF = efx_air_absorption_gain_hf;
#endif // USE_EAX20_HARDWARE_FILTERS
	}

	void set_efx_eax_reverb_properties(
		const ALuint oal_effect,
		const EFXEAXREVERBPROPERTIES& efx_reverb)
	{
		alEffectf_(oal_effect, AL_EAXREVERB_DENSITY, efx_reverb.flDensity);
		alEffectf_(oal_effect, AL_EAXREVERB_DIFFUSION, efx_reverb.flDiffusion);
		alEffectf_(oal_effect, AL_EAXREVERB_GAIN, efx_reverb.flGain);
		alEffectf_(oal_effect, AL_EAXREVERB_GAINHF, efx_reverb.flGainHF);
		alEffectf_(oal_effect, AL_EAXREVERB_GAINLF, efx_reverb.flGainLF);
		alEffectf_(oal_effect, AL_EAXREVERB_DECAY_TIME, efx_reverb.flDecayTime);
		alEffectf_(oal_effect, AL_EAXREVERB_DECAY_HFRATIO, efx_reverb.flDecayHFRatio);
		alEffectf_(oal_effect, AL_EAXREVERB_DECAY_LFRATIO, efx_reverb.flDecayLFRatio);
		alEffectf_(oal_effect, AL_EAXREVERB_REFLECTIONS_GAIN, efx_reverb.flReflectionsGain);
		alEffectf_(oal_effect, AL_EAXREVERB_REFLECTIONS_DELAY, efx_reverb.flReflectionsDelay);
		alEffectfv_(oal_effect, AL_EAXREVERB_REFLECTIONS_PAN, efx_reverb.flReflectionsPan);
		alEffectf_(oal_effect, AL_EAXREVERB_LATE_REVERB_GAIN, efx_reverb.flLateReverbGain);
		alEffectf_(oal_effect, AL_EAXREVERB_LATE_REVERB_DELAY, efx_reverb.flLateReverbDelay);
		alEffectfv_(oal_effect, AL_EAXREVERB_LATE_REVERB_PAN, efx_reverb.flLateReverbPan);
		alEffectf_(oal_effect, AL_EAXREVERB_ECHO_TIME, efx_reverb.flEchoTime);
		alEffectf_(oal_effect, AL_EAXREVERB_ECHO_DEPTH, efx_reverb.flEchoDepth);
		alEffectf_(oal_effect, AL_EAXREVERB_MODULATION_TIME, efx_reverb.flModulationTime);
		alEffectf_(oal_effect, AL_EAXREVERB_MODULATION_DEPTH, efx_reverb.flModulationDepth);
		alEffectf_(oal_effect, AL_EAXREVERB_HFREFERENCE, efx_reverb.flHFReference);
		alEffectf_(oal_effect, AL_EAXREVERB_LFREFERENCE, efx_reverb.flLFReference);
		alEffectf_(oal_effect, AL_EAXREVERB_ROOM_ROLLOFF_FACTOR, efx_reverb.flRoomRolloffFactor);
		alEffectf_(oal_effect, AL_EAXREVERB_AIR_ABSORPTION_GAINHF, efx_reverb.flAirAbsorptionGainHF);
		alEffecti_(oal_effect, AL_EAXREVERB_DECAY_HFLIMIT, efx_reverb.iDecayHFLimit);
	}

	void set_efx_reverb_properties(
		const ALuint oal_effect,
		const EFXEAXREVERBPROPERTIES& efx_reverb)
	{
		alEffectf_(oal_effect, AL_REVERB_DENSITY, efx_reverb.flDensity);
		alEffectf_(oal_effect, AL_REVERB_DIFFUSION, efx_reverb.flDiffusion);
		alEffectf_(oal_effect, AL_REVERB_GAIN, efx_reverb.flGain);
		alEffectf_(oal_effect, AL_REVERB_GAINHF, efx_reverb.flGainHF);
		alEffectf_(oal_effect, AL_REVERB_DECAY_TIME, efx_reverb.flDecayTime);
		alEffectf_(oal_effect, AL_REVERB_DECAY_HFRATIO, efx_reverb.flDecayHFRatio);
		alEffectf_(oal_effect, AL_REVERB_REFLECTIONS_GAIN, efx_reverb.flReflectionsGain);
		alEffectf_(oal_effect, AL_REVERB_REFLECTIONS_DELAY, efx_reverb.flReflectionsDelay);
		alEffectf_(oal_effect, AL_REVERB_LATE_REVERB_GAIN, efx_reverb.flLateReverbGain);
		alEffectf_(oal_effect, AL_REVERB_LATE_REVERB_DELAY, efx_reverb.flLateReverbDelay);
		alEffectf_(oal_effect, AL_REVERB_ROOM_ROLLOFF_FACTOR, efx_reverb.flRoomRolloffFactor);
		alEffectf_(oal_effect, AL_REVERB_AIR_ABSORPTION_GAINHF, efx_reverb.flAirAbsorptionGainHF);
		alEffecti_(oal_effect, AL_REVERB_DECAY_HFLIMIT, efx_reverb.iDecayHFLimit);
	}

	//
	// =========================================================================
	// Utils
	// =========================================================================


	// =========================================================================
	// OpenAL
	// =========================================================================
	//

	struct OalVersion
	{
		int major_;
		int minor_;


		constexpr bool is_empty() const
		{
			return major_ == 0 && minor_ == 0;
		}

		constexpr bool operator<(
			const OalVersion& that)
		{
			return major_ < that.major_ || (major_ == that.major_ && minor_ < that.minor_);
		}
	}; // OalVersion


	static constexpr auto oal_ref_version = OalVersion{1, 1};
	static constexpr auto oal_efx_ref_version = OalVersion{1, 0};


	const char* api_last_error() const
	{
		return error_message_.c_str();
	}

	static void oal_clear_error()
	{
		static_cast<void>(::alGetError());
	}

	static bool oal_is_succeed()
	{
		const auto error_code = ::alGetError();

		if (error_code != AL_NO_ERROR)
		{
			return false;
		}

		return true;
	}

	OalVersion oal_get_version(
		const ALenum oal_major_id,
		const ALenum oal_minor_id)
	{
		oal_clear_error();

		auto major = ALCint{};
		auto minor = ALCint{};

		::alcGetIntegerv(oal_device_, oal_major_id, 1, &major);
		::alcGetIntegerv(oal_device_, oal_minor_id, 1, &minor);

		if (!oal_is_succeed())
		{
			return {};
		}

		return {major, minor};
	}

	ExtensionsStrings oal_parse_extensions_string(
		const char* const extensions_string)
	{
		auto iss = std::istringstream{extensions_string};

		return ExtensionsStrings{
			std::istream_iterator<String>{iss},
			std::istream_iterator<String>{}};
	}

	bool oal_get_global_strings()
	{
		oal_clear_error();

		auto version_string = ::alGetString(AL_VERSION);
		auto renderer_string = ::alGetString(AL_RENDERER);
		auto vendor_string = ::alGetString(AL_VENDOR);
		auto extensions_string = ::alGetString(AL_EXTENSIONS);

		if (!oal_is_succeed() || !version_string || !renderer_string || !vendor_string || !extensions_string)
		{
			error_message_ = "OAL: Failed to get global strings.";
			return false;
		}

		oal_version_string_ = version_string;
		oal_renderer_string_ = renderer_string;
		oal_vendor_string_ = vendor_string;
		oal_extentions_strings_ = oal_parse_extensions_string(extensions_string);

		return true;
	}

	bool oal_get_version()
	{
		oal_version_ = oal_get_version(ALC_MAJOR_VERSION, ALC_MINOR_VERSION);

		if (oal_version_.is_empty())
		{
			error_message_ = "OAL: Failed to get a version.";
			return false;
		}

		if (oal_version_ < oal_ref_version)
		{
			error_message_ = "OAL: Expected at least version 1.1.";
			return false;
		}

		return true;
	}

	bool oal_get_efx_version()
	{
		oal_efx_version_ = oal_get_version(ALC_EFX_MAJOR_VERSION, ALC_EFX_MINOR_VERSION);

		if (oal_efx_version_.is_empty() || oal_efx_version_ < oal_efx_ref_version)
		{
			return false;
		}

		return true;
	}

	bool oal_get_max_aux_sends()
	{
		oal_clear_error();

		auto max_aux_sends = ALCint{};
		::alcGetIntegerv(oal_device_, ALC_MAX_AUXILIARY_SENDS, 1, &max_aux_sends);

		if (!oal_is_succeed() || max_aux_sends == 0)
		{
			return false;
		}

		oal_max_aux_sends_ = max_aux_sends;

		return true;
	}

	template<typename T>
	static bool oal_get_efx_function(
		const char* name,
		T& function)
	{
		function = reinterpret_cast<T>(::alGetProcAddress(name));

		return function;
	}

	void oal_clear_efx_functions()
	{
		alGenEffects_ = nullptr;
		alDeleteEffects_ = nullptr;
		alIsEffect_ = nullptr;
		alEffecti_ = nullptr;
		alEffectiv_ = nullptr;
		alEffectf_ = nullptr;
		alEffectfv_ = nullptr;
		alGetEffecti_ = nullptr;
		alGetEffectiv_ = nullptr;
		alGetEffectf_ = nullptr;
		alGetEffectfv_ = nullptr;
		alGenFilters_ = nullptr;
		alDeleteFilters_ = nullptr;
		alIsFilter_ = nullptr;
		alFilteri_ = nullptr;
		alFilteriv_ = nullptr;
		alFilterf_ = nullptr;
		alFilterfv_ = nullptr;
		alGetFilteri_ = nullptr;
		alGetFilteriv_ = nullptr;
		alGetFilterf_ = nullptr;
		alGetFilterfv_ = nullptr;
		alGenAuxiliaryEffectSlots_ = nullptr;
		alDeleteAuxiliaryEffectSlots_ = nullptr;
		alIsAuxiliaryEffectSlot_ = nullptr;
		alAuxiliaryEffectSloti_ = nullptr;
		alAuxiliaryEffectSlotiv_ = nullptr;
		alAuxiliaryEffectSlotf_ = nullptr;
		alAuxiliaryEffectSlotfv_ = nullptr;
		alGetAuxiliaryEffectSloti_ = nullptr;
		alGetAuxiliaryEffectSlotiv_ = nullptr;
		alGetAuxiliaryEffectSlotf_ = nullptr;
		alGetAuxiliaryEffectSlotfv_ = nullptr;
	}

	bool oal_get_efx_symbols()
	{
		auto result = true;

		result &= oal_get_efx_function("alGenEffects", alGenEffects_);
		result &= oal_get_efx_function("alDeleteEffects", alDeleteEffects_);
		result &= oal_get_efx_function("alIsEffect", alIsEffect_);
		result &= oal_get_efx_function("alEffecti", alEffecti_);
		result &= oal_get_efx_function("alEffectiv", alEffectiv_);
		result &= oal_get_efx_function("alEffectf", alEffectf_);
		result &= oal_get_efx_function("alEffectfv", alEffectfv_);
		result &= oal_get_efx_function("alGetEffecti", alGetEffecti_);
		result &= oal_get_efx_function("alGetEffectiv", alGetEffectiv_);
		result &= oal_get_efx_function("alGetEffectf", alGetEffectf_);
		result &= oal_get_efx_function("alGetEffectfv", alGetEffectfv_);
		result &= oal_get_efx_function("alGenFilters", alGenFilters_);
		result &= oal_get_efx_function("alDeleteFilters", alDeleteFilters_);
		result &= oal_get_efx_function("alIsFilter", alIsFilter_);
		result &= oal_get_efx_function("alFilteri", alFilteri_);
		result &= oal_get_efx_function("alFilteriv", alFilteriv_);
		result &= oal_get_efx_function("alFilterf", alFilterf_);
		result &= oal_get_efx_function("alFilterfv", alFilterfv_);
		result &= oal_get_efx_function("alGetFilteri", alGetFilteri_);
		result &= oal_get_efx_function("alGetFilteriv", alGetFilteriv_);
		result &= oal_get_efx_function("alGetFilterf", alGetFilterf_);
		result &= oal_get_efx_function("alGetFilterfv", alGetFilterfv_);
		result &= oal_get_efx_function("alGenAuxiliaryEffectSlots", alGenAuxiliaryEffectSlots_);
		result &= oal_get_efx_function("alDeleteAuxiliaryEffectSlots", alDeleteAuxiliaryEffectSlots_);
		result &= oal_get_efx_function("alIsAuxiliaryEffectSlot", alIsAuxiliaryEffectSlot_);
		result &= oal_get_efx_function("alAuxiliaryEffectSloti", alAuxiliaryEffectSloti_);
		result &= oal_get_efx_function("alAuxiliaryEffectSlotiv", alAuxiliaryEffectSlotiv_);
		result &= oal_get_efx_function("alAuxiliaryEffectSlotf", alAuxiliaryEffectSlotf_);
		result &= oal_get_efx_function("alAuxiliaryEffectSlotfv", alAuxiliaryEffectSlotfv_);
		result &= oal_get_efx_function("alGetAuxiliaryEffectSloti", alGetAuxiliaryEffectSloti_);
		result &= oal_get_efx_function("alGetAuxiliaryEffectSlotiv", alGetAuxiliaryEffectSlotiv_);
		result &= oal_get_efx_function("alGetAuxiliaryEffectSlotf", alGetAuxiliaryEffectSlotf_);
		result &= oal_get_efx_function("alGetAuxiliaryEffectSlotfv", alGetAuxiliaryEffectSlotfv_);

		return result;
	}

	bool oal_has_efx()
	{
		if (!::alcIsExtensionPresent(oal_device_, ALC_EXT_EFX_NAME))
		{
			return false;
		}

		return true;
	}

	bool oal_detect_effect(
		const ALenum effect_type)
	{
		oal_clear_error();

		auto has_effect = false;
		auto effect_name = ALuint{};

		alGenEffects_(1, &effect_name);

		if (oal_is_succeed())
		{
			alEffecti_(effect_name, AL_EFFECT_TYPE, effect_type);

			if (oal_is_succeed())
			{
				has_effect = true;
			}
		}

		if (alIsEffect_(effect_name))
		{
			alDeleteEffects_(1, &effect_name);
		}

		return has_effect;
	}

	void oal_detect_effects()
	{
		oal_has_chorus_effect_ = oal_detect_effect(AL_EFFECT_CHORUS);
		oal_has_compressor_effect_ = oal_detect_effect(AL_EFFECT_COMPRESSOR);
		oal_has_distortion_effect_ = oal_detect_effect(AL_EFFECT_DISTORTION);
		oal_has_echo_effect_ = oal_detect_effect(AL_EFFECT_ECHO);
		oal_has_flange_effect_ = oal_detect_effect(AL_EFFECT_FLANGER);
		oal_has_equalizer_effect_ = oal_detect_effect(AL_EFFECT_EQUALIZER);
		oal_has_reverb_effect_ = oal_detect_effect(AL_EFFECT_REVERB);
		oal_has_eax_reverb_effect_ = oal_detect_effect(AL_EFFECT_EAXREVERB);
	}

	void oal_examine_efx()
	{
		auto is_succeed = true;

		auto guard_this = ul::ScopeGuard{
			[&]()
			{
				if (!is_succeed)
				{
					oal_clear_efx();
				}
			}
		};

		if (!oal_get_efx_version())
		{
			return;
		}

		if (!oal_get_max_aux_sends())
		{
			return;
		}

		if (!oal_get_efx_symbols())
		{
			return;
		}

		oal_detect_effects();

		is_succeed = true;
		oal_has_efx_ = true;
	}

	void oal_clear_efx()
	{
		oal_efx_version_ = {};

		oal_has_efx_ = false;

		oal_has_chorus_effect_ = false;
		oal_has_compressor_effect_ = false;
		oal_has_distortion_effect_ = false;
		oal_has_echo_effect_ = false;
		oal_has_flange_effect_ = false;
		oal_has_equalizer_effect_ = false;
		oal_has_reverb_effect_ = false;
		oal_has_eax_reverb_effect_ = false;
		oal_max_aux_sends_ = 0;

		oal_clear_efx_functions();
	}

	bool oal_open_device()
	{
		oal_device_ = ::alcOpenDevice(nullptr);

		if (!oal_device_)
		{
			error_message_ = "OAL: Failed to open a device.";
			return false;
		}

		return true;
	}

	void oal_close_device()
	{
		if (oal_device_)
		{
			static_cast<void>(::alcCloseDevice(oal_device_));
			oal_device_ = nullptr;
		}
	}

	bool oal_create_context()
	{
		const auto attributes = OalContextAttributes{
			ALC_MAX_AUXILIARY_SENDS, default_aux_sends,
			0, 0,
		}; // attributes

		oal_context_ = ::alcCreateContext(oal_device_, attributes.data());

		if (!oal_context_)
		{
			error_message_ = "OAL: Failed to create a context.";
			return false;
		}

		if (!::alcMakeContextCurrent(oal_context_))
		{
			error_message_ = "OAL: Failed to make a context current.";
			return false;
		}

		return true;
	}

	bool oal_get_context_attributes()
	{
		auto attributes_size = ALCint{};

		::alcGetIntegerv(oal_device_, ALC_ATTRIBUTES_SIZE, 1, &attributes_size);

		if (!oal_is_succeed())
		{
			error_message_ = "OAL: Failed to get context attributes size.";
			return false;
		}

		auto attributes = OalContextAttributes{};
		attributes.resize(attributes_size);

		::alcGetIntegerv(oal_device_, ALC_ALL_ATTRIBUTES, attributes_size, attributes.data());

		if (!oal_is_succeed())
		{
			error_message_ = "OAL: Failed to get context attributes.";
			return false;
		}

		const auto attribute_count = attributes_size / 2;

		for (auto i = 0; i < attribute_count; ++i)
		{
			const auto attribute_index = i * 2;
			const auto attribute_id = attributes[attribute_index + 0];
			const auto attribute_value = attributes[attribute_index + 1];

			switch (attribute_id)
			{
			case ALC_FREQUENCY:
				oal_context_sample_rate_ = attribute_value;
				break;

			default:
				break;
			}
		}

		return true;
	}

	void oal_destroy_context()
	{
		if (oal_context_)
		{
			static_cast<void>(::alcMakeContextCurrent(nullptr));
			::alcDestroyContext(oal_context_);
			oal_context_ = nullptr;
		}
	}

	//
	// =========================================================================
	// OpenAL
	// =========================================================================


	// =========================================================================
	// API utils
	// =========================================================================
	//

	void remove_samples()
	{
		samples_.clear();
		objects_3d_.clear();
	}

	bool wave_out_open_internal()
	{
		auto is_succeed = false;

		auto guard_this = ul::ScopeGuard{
			[&]()
			{
				if (!is_succeed)
				{
					wave_out_close_internal();
				}
			}
		};

		if (!oal_open_device())
		{
			return false;
		}

		if (!oal_create_context())
		{
			return false;
		}

		if (!oal_get_context_attributes())
		{
			return false;
		}

		if (!oal_get_version())
		{
			return false;
		}

		if (!oal_get_global_strings())
		{
			return false;
		}

		oal_examine_efx();

		initialize_eax20_filter();

		create_streams();

		is_succeed = true;
		master_volume_ = ltjs::AudioUtils::lt_max_volume;
		oal_master_gain_ = default_gain;

		listener_3d_uptr_ = std::make_unique<StreamingSource>(
			StreamingSource::Type::spatial, StreamingSource::SpatialType::listener);

		return true;
	}

	void wave_out_close_internal()
	{
		uninitialize_eax20_filter();

		destroy_streams();

		remove_samples();

		listener_3d_uptr_ = {};

		oal_destroy_context();
		oal_close_device();
		oal_clear_efx();

		master_volume_ = {};
		oal_master_gain_ = {};
	}


	static bool validate_wave_format_ex(
		const ul::WaveFormatEx& wave_format)
	{
		if (wave_format.tag_ != ul::WaveFormatTag::pcm)
		{
			return false;
		}

		if (wave_format.channel_count_ != 1 && wave_format.channel_count_ != 2)
		{
			return false;
		}

		if (wave_format.bit_depth_ != 8 && wave_format.bit_depth_ != 16)
		{
			return false;
		}

		if (wave_format.sample_rate_ <= 0)
		{
			return false;
		}

		return true;
	}

	static ALenum get_oal_buffer_format(
		const int channel_count,
		const int bit_depth)
	{
		switch (channel_count)
		{
		case 1:
			switch (bit_depth)
			{
			case 8:
				return AL_FORMAT_MONO8;

			case 16:
				return AL_FORMAT_MONO16;

			default:
				return AL_NONE;
			}

			break;


		case 2:
			switch (bit_depth)
			{
			case 8:
				return AL_FORMAT_STEREO8;

			case 16:
				return AL_FORMAT_STEREO16;

			default:
				return AL_NONE;
			}

			break;

		default:
			return AL_NONE;
		}
	}

	static ALenum get_oal_buffer_format(
		const ul::WaveFormatEx& wave_format)
	{
		return get_oal_buffer_format(wave_format.channel_count_, wave_format.bit_depth_);
	}

	void set_set_stream_loop(
		LHSTREAM stream_ptr,
		const bool is_enable)
	{
		if (!stream_ptr)
		{
			return;
		}

		auto& stream = *static_cast<StreamingSource*>(stream_ptr);

		MtMutexGuard mt_stream_lock{mt_streams_mutex_};

		stream.is_looping_ = is_enable;
	}

	void uninitialize_eax20_filter()
	{
		if (oal_effect_slot_ != AL_EFFECTSLOT_NULL)
		{
			alAuxiliaryEffectSloti_(oal_effect_slot_, AL_EFFECTSLOT_EFFECT, AL_EFFECT_NULL);

			alDeleteAuxiliaryEffectSlots_(1, &oal_effect_slot_);
			oal_effect_slot_ = AL_EFFECTSLOT_NULL;
		}

		if (oal_effect_ != AL_EFFECT_NULL)
		{
			alDeleteEffects_(1, &oal_effect_);
			oal_effect_ = AL_EFFECT_NULL;
		}

		if (oal_null_effect_ != AL_EFFECT_NULL)
		{
			alDeleteEffects_(1, &oal_null_effect_);
			oal_null_effect_ = AL_EFFECT_NULL;
		}

		oal_is_supports_eax20_filter_ = false;
	}

	void oal_update_reverb_effect()
	{
		if (oal_has_eax_reverb_effect_)
		{
			set_efx_eax_reverb_properties(oal_effect_, oal_efx_eax_reverb_properties_);
		}
		else
		{
			set_efx_reverb_properties(oal_effect_, oal_efx_eax_reverb_properties_);
		}
	}

	void initialize_eax20_filter()
	{
		if (!is_eax20_filters_defined_ ||
			!oal_has_efx_ ||
			oal_max_aux_sends_ <= 0 ||
			!(oal_has_reverb_effect_ || oal_has_eax_reverb_effect_))
		{
			return;
		}

		uninitialize_eax20_filter();

		oal_clear_error();

		alGenEffects_(1, &oal_effect_);
		alGenEffects_(1, &oal_null_effect_);
		alGenAuxiliaryEffectSlots_(1, &oal_effect_slot_);

		const auto oal_effect_type = (oal_has_eax_reverb_effect_ ? AL_EFFECT_EAXREVERB : AL_EFFECT_REVERB);

		alEffecti_(oal_effect_, AL_EFFECT_TYPE, oal_effect_type);

		if (!oal_is_succeed())
		{
			uninitialize_eax20_filter();
			return;
		}

		oal_is_supports_eax20_filter_ = true;
	}

	//
	// =========================================================================
	// API utils
	// =========================================================================


	// =========================================================================
	// API
	// =========================================================================
	//

	sint32 api_startup()
	{
#ifdef USE_EAX20_HARDWARE_FILTERS
		is_eax20_filters_defined_ = true;
#else
		is_eax20_filters_defined_ = false;
#endif // USE_EAX20_HARDWARE_FILTERS

		clock_base_ = Clock::now();

		return LS_OK;
	}

	void api_shutdown()
	{
	}

	std::uint32_t api_ms_count()
	{
		constexpr auto max_uint32_t = std::numeric_limits<std::uint32_t>::max();

		const auto time_diff = Clock::now() - clock_base_;
		const auto time_diff_ms = std::chrono::duration_cast<std::chrono::milliseconds>(time_diff).count();

		return static_cast<std::uint32_t>(time_diff_ms % max_uint32_t);
	}

	sint32 api_wave_out_open(
		LHDIGDRIVER& driver,
		PHWAVEOUT& wave_out,
		const sint32 device_id,
		const ul::WaveFormatEx& wave_format)
	{
		static_cast<void>(device_id);
		static_cast<void>(wave_format);

		driver = nullptr;
		wave_out = nullptr;

		wave_out_close_internal();

		if (!wave_out_open_internal())
		{
			return LS_ERROR;
		}

		driver = oal_device_;

		return LS_OK;
	}

	void api_wave_out_close(
		LHDIGDRIVER driver_ptr)
	{
		static_cast<void>(driver_ptr);

		wave_out_close_internal();
	}

	sint32 api_get_digital_master_volume(
		LHDIGDRIVER driver_ptr) const
	{
		if (!driver_ptr || driver_ptr != oal_device_)
		{
			return {};
		}

		return master_volume_;
	}

	void api_set_digital_master_volume(
		LHDIGDRIVER driver_ptr,
		const sint32 master_volume)
	{
		if (!driver_ptr || driver_ptr != oal_device_)
		{
			return;
		}

		listener_3d_uptr_->set_master_3d_listener_volume(master_volume);
	}

	LHSAMPLE api_allocate_sample_handle(
		LHDIGDRIVER driver_ptr)
	{
		if (driver_ptr != oal_device_)
		{
			return nullptr;
		}

		samples_.emplace_back(StreamingSource::Type::panning, StreamingSource::SpatialType::none);

		auto& source = samples_.back();

		MtMutexGuard lock{mt_samples_mutex_};
		mt_open_samples_.emplace_back(&source);

		return &source;
	}

	void api_release_sample_handle(
		LHSAMPLE sample_ptr)
	{
		if (!sample_ptr)
		{
			return;
		}

		auto& source = *static_cast<StreamingSource*>(sample_ptr);

		{
			MtMutexGuard lock{mt_samples_mutex_};

			mt_open_samples_.remove_if(
				[&](const auto& item)
				{
				return &source == item;
				}
			);
		}

		samples_.remove_if(
			[&](const auto& item)
			{
			return &source == &item;
			}
		);
	}

	sint32 api_init_sample_from_address(
		LHSAMPLE sample_handle,
		const void* storage_ptr,
		const uint32 storage_size,
		const ul::WaveFormatEx& wave_format,
		const sint32 playback_rate,
		const LTSOUNDFILTERDATA* filter_data_ptr)
	{
		static_cast<void>(filter_data_ptr);

		if (!sample_handle)
		{
			return false;
		}

		auto& source = *static_cast<StreamingSource*>(sample_handle);

		if (source.is_failed())
		{
			return false;
		}

		auto open_parameters = StreamingSource::OpenParameters{};

		open_parameters.is_memory_ = true;
		open_parameters.memory_ptr_ = storage_ptr;
		open_parameters.memory_size_ = storage_size;
		open_parameters.memory_wave_format_ = wave_format;
		open_parameters.playback_rate_ = playback_rate;
		open_parameters.oal_has_effect_slot_ = oal_is_supports_eax20_filter_;
		open_parameters.oal_has_effect_slot_ = oal_effect_slot_;

		{
			MtMutexGuard lock{mt_samples_mutex_};

			if (!source.open(open_parameters))
			{
				return false;
			}
		}

		mt_notify_stream();

		return true;
	}

	sint32 api_init_sample_from_file(
		LHSAMPLE sample_handle,
		const void* storage_ptr,
		const sint32 block,
		const sint32 playback_rate,
		const LTSOUNDFILTERDATA* filter_data_ptr)
	{
		static_cast<void>(block);
		static_cast<void>(filter_data_ptr);

		if (!sample_handle)
		{
			return false;
		}

		auto& source = *static_cast<StreamingSource*>(sample_handle);

		if (source.is_failed())
		{
			return false;
		}

		auto open_parameters = StreamingSource::OpenParameters{};

		open_parameters.is_mapped_file_ = true;
		open_parameters.mapped_decoder_ = &audio_decoder_;
		open_parameters.mapped_storage_ptr = storage_ptr;
		open_parameters.playback_rate_ = playback_rate;
		open_parameters.oal_has_effect_slot_ = oal_is_supports_eax20_filter_;
		open_parameters.oal_has_effect_slot_ = oal_effect_slot_;

		{
			MtMutexGuard lock{mt_samples_mutex_};

			if (!source.open(open_parameters))
			{
				return false;
			}
		}

		mt_notify_stream();

		return true;
	}

	void api_stop_sample(
		LHSAMPLE sample_ptr)
	{
		if (!sample_ptr)
		{
			return;
		}

		auto& sample = *static_cast<StreamingSource*>(sample_ptr);

		{
			MtMutexGuard lock{mt_samples_mutex_};
			sample.pause();
		}

		mt_notify_stream();
	}

	void api_start_sample(
		LHSAMPLE sample_ptr)
	{
		if (!sample_ptr)
		{
			return;
		}

		auto& sample = *static_cast<StreamingSource*>(sample_ptr);

		{
			MtMutexGuard lock{mt_samples_mutex_};

			sample.stop();
			sample.resume();
		}

		mt_notify_stream();
	}

	void api_resume_sample(
		LHSAMPLE sample_ptr)
	{
		if (!sample_ptr)
		{
			return;
		}

		auto& sample = *static_cast<StreamingSource*>(sample_ptr);

		{
			MtMutexGuard lock{mt_samples_mutex_};
			sample.resume();
		}

		mt_notify_stream();
	}

	void api_end_sample(
		LHSAMPLE sample_ptr)
	{
		if (!sample_ptr)
		{
			return;
		}

		auto& sample = *static_cast<StreamingSource*>(sample_ptr);

		MtMutexGuard lock{mt_samples_mutex_};

		sample.stop();
	}

	sint32 api_get_sample_volume(
		LHSAMPLE sample_ptr)
	{
		if (!sample_ptr)
		{
			return {};
		}

		auto& sample = *static_cast<StreamingSource*>(sample_ptr);

		MtMutexGuard lock{mt_samples_mutex_};

		return sample.volume_;
	}

	void api_set_sample_volume(
		LHSAMPLE sample_ptr,
		const sint32 volume)
	{
		if (!sample_ptr)
		{
			return;
		}

		auto& sample = *static_cast<StreamingSource*>(sample_ptr);

		MtMutexGuard lock{mt_samples_mutex_};

		sample.set_volume(volume);
	}

	sint32 api_get_sample_pan(
		LHSAMPLE sample_ptr)
	{
		if (!sample_ptr)
		{
			return {};
		}

		auto& sample = *static_cast<StreamingSource*>(sample_ptr);

		MtMutexGuard lock{mt_samples_mutex_};

		return sample.pan_;
	}

	void api_set_sample_pan(
		LHSAMPLE sample_ptr,
		const sint32 pan)
	{
		if (!sample_ptr)
		{
			return;
		}

		auto& sample = *static_cast<StreamingSource*>(sample_ptr);

		MtMutexGuard lock{mt_samples_mutex_};

		sample.set_pan(pan);
	}

	sint32 api_get_sample_user_data(
		LHSAMPLE sample_handle,
		const uint32 index)
	{
		if (!sample_handle || index > max_user_data_index)
		{
			return {};
		}


		auto& sample = *static_cast<StreamingSource*>(sample_handle);

		MtMutexGuard lock{mt_samples_mutex_};

		return sample.user_data_array_[index];
	}

	void api_set_sample_user_data(
		LHSAMPLE sample_handle,
		const uint32 index,
		const sint32 value)
	{
		if (!sample_handle || index > max_user_data_index)
		{
			return;
		}

		auto& sample = *static_cast<StreamingSource*>(sample_handle);

		MtMutexGuard lock{mt_samples_mutex_};

		sample.user_data_array_[index] = value;
	}

	void api_set_sample_loop_block(
		LHSAMPLE sample_handle,
		const sint32 loop_begin_offset,
		const sint32 loop_end_offset,
		const bool is_enable)
	{
		if (!sample_handle)
		{
			return;
		}

		auto& sample = *static_cast<StreamingSource*>(sample_handle);

		MtMutexGuard lock{mt_samples_mutex_};

		sample.set_loop_block(loop_begin_offset, loop_end_offset, is_enable);
	}

	void api_set_sample_loop(
		LHSAMPLE sample_handle,
		const bool is_enable)
	{
		if (!sample_handle)
		{
			return;
		}

		auto& sample = *static_cast<StreamingSource*>(sample_handle);

		MtMutexGuard lock{mt_samples_mutex_};

		sample.set_loop(is_enable);
	}

	void api_set_sample_ms_position(
		LHSAMPLE sample_handle,
		const sint32 milliseconds)
	{
		if (!sample_handle)
		{
			return;
		}

		auto& sample = *static_cast<StreamingSource*>(sample_handle);

		MtMutexGuard lock{mt_samples_mutex_};

		sample.set_ms_position(milliseconds);
	}

	uint32 api_get_sample_status(
		LHSAMPLE sample_handle)
	{
		if (!sample_handle)
		{
			return {};
		}

		auto& sample = *static_cast<StreamingSource*>(sample_handle);

		MtMutexGuard lock{mt_samples_mutex_};

		if (sample.is_failed())
		{
			return LS_ERROR;
		}

		return sample.is_playing() ? LS_PLAYING : LS_STOPPED;
	}

	LHSTREAM api_open_stream(
		const char* file_name,
		const uint32 file_offset,
		LHDIGDRIVER driver_ptr,
		const char* storage_ptr,
		const sint32 storage_size)
	{
		static_cast<void>(storage_ptr);
		static_cast<void>(storage_size);

		if (!file_name || driver_ptr != oal_device_)
		{
			return nullptr;
		}

		streams_.emplace_back(StreamingSource::Type::panning, StreamingSource::SpatialType::none);

		auto& source = streams_.back();

		if (source.is_failed())
		{
			streams_.pop_back();
			return nullptr;
		}

		auto open_parameters = StreamingSource::OpenParameters{};

		open_parameters.is_file_ = true;
		open_parameters.file_name_ = file_name;
		open_parameters.file_offset_ = file_offset;
		open_parameters.oal_has_effect_slot_ = oal_is_supports_eax20_filter_;
		open_parameters.oal_has_effect_slot_ = oal_effect_slot_;

		if (!source.open(open_parameters))
		{
			streams_.pop_back();
			return false;
		}

		{
			MtMutexGuard lock{mt_streams_mutex_};
			mt_open_streams_.emplace_back(&source);
		}

		mt_notify_stream();

		return &source;
	}

	void api_close_stream(
		LHSTREAM stream_ptr)
	{
		if (!stream_ptr)
		{
			return;
		}

		auto& stream = *static_cast<StreamingSource*>(stream_ptr);

		{
			MtMutexGuard mt_stream_lock{mt_streams_mutex_};

			mt_open_streams_.remove_if(
				[&](const auto& item)
				{
					return &stream == item;
				}
			);
		}

		streams_.remove_if(
			[&](const auto& item)
			{
				return &stream == &item;
			}
		);
	}

	void api_set_stream_ms_position(
		LHSTREAM stream_ptr,
		const sint32 milliseconds)
	{
		if (!stream_ptr || milliseconds < 0)
		{
			return;
		}

		auto& stream = *static_cast<StreamingSource*>(stream_ptr);

		MtMutexGuard mt_stream_lock{mt_streams_mutex_};

		const auto block_align = stream.get_block_align();
		const auto sample_rate = stream.get_dst_sample_rate();

		auto position = static_cast<int>((milliseconds * sample_rate) / 1000LL);
		position /= block_align;
		position *= block_align;

		if (position > stream.data_size_)
		{
			return;
		}

		stream.data_offset_ = position;
	}

	void api_set_stream_user_data(
		LHSTREAM stream_ptr,
		const uint32 index,
		const sint32 value)
	{
		if (!stream_ptr || index > max_user_data_index)
		{
			return;
		}

		auto& stream = *static_cast<StreamingSource*>(stream_ptr);

		stream.user_data_array_[index] = value;
	}

	sint32 api_get_stream_user_data(
		LHSTREAM stream_ptr,
		const uint32 index)
	{
		if (!stream_ptr || index > max_user_data_index)
		{
			return {};
		}

		auto& stream = *static_cast<StreamingSource*>(stream_ptr);

		return stream.user_data_array_[index];
	}

	void api_start_stream(
		LHSTREAM stream_ptr)
	{
		if (!stream_ptr)
		{
			return;
		}

		auto& stream = *static_cast<StreamingSource*>(stream_ptr);

		{
			MtMutexGuard mt_stream_lock{mt_streams_mutex_};

			stream.stop();
			stream.resume();
		}

		mt_notify_stream();
	}

	void api_pause_stream(
		LHSTREAM stream_ptr,
		const sint32 is_enable)
	{
		if (!stream_ptr)
		{
			return;
		}

		auto& stream = *static_cast<StreamingSource*>(stream_ptr);

		{
			MtMutexGuard mt_stream_lock{mt_streams_mutex_};

			if (is_enable)
			{
				stream.pause();
			}
			else
			{
				stream.resume();
			}
		}

		mt_notify_stream();
	}

	void api_set_stream_volume(
		LHSTREAM stream_ptr,
		const sint32 volume)
	{
		if (!stream_ptr)
		{
			return;
		}

		auto& stream = *static_cast<StreamingSource*>(stream_ptr);

		MtMutexGuard mt_stream_lock{mt_streams_mutex_};

		stream.set_volume(volume);
	}

	void api_set_stream_pan(
		LHSTREAM stream_ptr,
		const sint32 pan)
	{
		if (!stream_ptr)
		{
			return;
		}

		auto& stream = *static_cast<StreamingSource*>(stream_ptr);

		MtMutexGuard mt_stream_lock{mt_streams_mutex_};

		stream.set_pan(pan);
	}

	sint32 api_get_stream_volume(
		LHSTREAM stream_ptr)
	{
		if (!stream_ptr)
		{
			return {};
		}

		auto& stream = *static_cast<StreamingSource*>(stream_ptr);

		MtMutexGuard mt_stream_lock{mt_streams_mutex_};

		return stream.volume_;
	}

	sint32 api_get_stream_pan(
		LHSTREAM stream_ptr)
	{
		if (!stream_ptr)
		{
			return {};
		}

		auto& stream = *static_cast<StreamingSource*>(stream_ptr);

		MtMutexGuard mt_stream_lock{mt_streams_mutex_};

		return stream.pan_;
	}

	uint32 api_get_stream_status(
		LHSTREAM stream_ptr)
	{
		if (!stream_ptr)
		{
			return LS_ERROR;
		}

		auto& stream = *static_cast<StreamingSource*>(stream_ptr);

		MtMutexGuard mt_stream_lock{mt_streams_mutex_};

		return stream.is_playing() ? LS_PLAYING : LS_STOPPED;
	}

	sint32 api_decode_mp3(
		const void* src_data_ptr,
		const uint32 src_data_size,
		const char* file_name_ext,
		void*& ds_wav_ptr,
		uint32& ds_wav_size,
		LTLENGTHYCB callback)
	{
		static_cast<void>(file_name_ext);
		static_cast<void>(callback);

		return ltjs::AudioUtils::decode_mp3(audio_decoder_, src_data_ptr, src_data_size, ds_wav_ptr, ds_wav_size);
	}

	void api_get_3d_provider_attribute(
		LHPROVIDER provider_id,
		const char* name,
		void* value)
	{
		if (!value)
		{
			return;
		}

		auto& int_value = *static_cast<sint32*>(value);

		int_value = -1;

		if (!name)
		{
			return;
		}

		const auto name_string = std::string{name};

		if (name_string != "Max samples")
		{
			return;
		}

		int_value = 255;
	}

	LH3DPOBJECT api_open_3d_listener(
		LHPROVIDER provider_id)
	{
		static_cast<void>(provider_id);

		return listener_3d_uptr_.get();
	}

	void api_close_3d_listener(
		LH3DPOBJECT listener_ptr)
	{
		static_cast<void>(listener_ptr);
	}

	void api_set_listener_doppler(
		LH3DPOBJECT listener_ptr,
		const float doppler_factor)
	{
		if (listener_ptr != listener_3d_uptr_.get())
		{
			return;
		}

		auto& listener = *listener_3d_uptr_.get();

		listener.set_3d_doppler_factor(doppler_factor);
	}

	void api_set_3d_position(
		LH3DPOBJECT object_ptr,
		const float x,
		const float y,
		const float z)
	{
		if (!object_ptr)
		{
			return;
		}

		auto& source = *static_cast<StreamingSource*>(object_ptr);

		MtMutexGuard lock{mt_3d_objects_mutex_};

		source.set_3d_position({x, y, z});
	}

	void api_set_3d_velocity_vector(
		LH3DPOBJECT object_ptr,
		const float dx_per_s,
		const float dy_per_s,
		const float dz_per_s)
	{
		if (!object_ptr)
		{
			return;
		}

		auto& source = *static_cast<StreamingSource*>(object_ptr);

		MtMutexGuard lock{mt_3d_objects_mutex_};

		source.set_3d_velocity({dx_per_s, dy_per_s, dz_per_s});
	}

	void api_set_3d_orientation(
		LH3DPOBJECT object_ptr,
		const float x_face,
		const float y_face,
		const float z_face,
		const float x_up,
		const float y_up,
		const float z_up)
	{
		if (!object_ptr)
		{
			return;
		}

		auto& source = *static_cast<StreamingSource*>(object_ptr);

		MtMutexGuard lock{mt_3d_objects_mutex_};

		source.set_3d_orientation({x_face, y_face, z_face, x_up, y_up, z_up});
	}

	void api_get_3d_position(
		LH3DPOBJECT object_ptr,
		float& x,
		float& y,
		float& z)
	{
		if (!object_ptr)
		{
			x = 0.0F;
			y = 0.0F;
			z = 0.0F;

			return;
		}

		const auto& source = *static_cast<const StreamingSource*>(object_ptr);

		MtMutexGuard lock{mt_3d_objects_mutex_};

		const auto& position = source.get_3d_position();

		x = position[0];
		y = position[1];
		z = position[2];
	}

	void api_get_3d_velocity(
		LH3DPOBJECT object_ptr,
		float& dx_per_ms,
		float& dy_per_ms,
		float& dz_per_ms)
	{
		if (!object_ptr)
		{
			dx_per_ms = 0.0F;
			dy_per_ms = 0.0F;
			dz_per_ms = 0.0F;

			return;
		}

		const auto& source = *static_cast<const StreamingSource*>(object_ptr);

		MtMutexGuard lock{mt_3d_objects_mutex_};

		const auto& velocity = source.get_3d_velocity();

		dx_per_ms = velocity[0];
		dy_per_ms = velocity[1];
		dz_per_ms = velocity[2];
	}

	void api_get_3d_orientation(
		LH3DPOBJECT object_ptr,
		float& x_face,
		float& y_face,
		float& z_face,
		float& x_up,
		float& y_up,
		float& z_up)
	{
		if (!object_ptr)
		{
			x_face = 0.0F;
			y_face = 0.0F;
			z_face = 0.0F;

			x_up = 0.0F;
			y_up = 0.0F;
			z_up = 0.0F;

			return;
		}

		const auto& source = *static_cast<const StreamingSource*>(object_ptr);

		MtMutexGuard lock{mt_3d_objects_mutex_};

		const auto& orientation = source.get_3d_orientation();

		x_face = orientation[0];
		y_face = orientation[1];
		z_face = orientation[2];

		x_up = orientation[3];
		y_up = orientation[4];
		z_up = orientation[5];
	}

	LH3DSAMPLE api_allocate_3d_sample_handle(
		LHPROVIDER provider_id)
	{
		static_cast<void>(provider_id);

		objects_3d_.emplace_back(StreamingSource::Type::spatial, StreamingSource::SpatialType::source);

		auto& source = objects_3d_.back();

		{
			MtMutexGuard lock{mt_3d_objects_mutex_};
			mt_open_3d_objects_.emplace_back(&source);
		}

		return &source;
	}

	void api_release_3d_sample_handle(
		LH3DSAMPLE sample_handle)
	{
		if (!sample_handle)
		{
			return;
		}

		auto& source = *static_cast<StreamingSource*>(sample_handle);

		{
			MtMutexGuard lock{mt_3d_objects_mutex_};

			mt_open_3d_objects_.remove_if(
				[&](const auto& item)
				{
					return &source == item;
				}
			);
		}

		objects_3d_.remove_if(
			[&](const auto& item)
			{
				return &source == &item;
			}
		);
	}

	void api_set_3d_sample_volume(
		LH3DSAMPLE sample_handle,
		const sint32 volume)
	{
		auto& source = *static_cast<StreamingSource*>(sample_handle);

		MtMutexGuard lock{mt_3d_objects_mutex_};

		source.set_3d_volume(volume);
	}

	void api_set_3d_sample_distances(
		LH3DSAMPLE sample_handle,
		const float max_distance,
		const float min_distance)
	{
		if (!sample_handle)
		{
			return;
		}

		auto& source = *static_cast<StreamingSource*>(sample_handle);

		MtMutexGuard lock{mt_3d_objects_mutex_};

		source.set_3d_distances(min_distance, max_distance);
	}

	void api_set_3d_user_data(
		LH3DPOBJECT object_ptr,
		const uint32 index,
		const sint32 value)
	{
		if (!object_ptr || index > max_user_data_index)
		{
			return;
		}

		auto& source = *static_cast<StreamingSource*>(object_ptr);

		MtMutexGuard lock{mt_3d_objects_mutex_};

		source.user_data_array_[index] = value;
	}

	sint32 api_get_3d_user_data(
		LH3DPOBJECT object_ptr,
		const uint32 index)
	{
		if (!object_ptr || index > max_user_data_index)
		{
			return {};
		}

		auto& source = *static_cast<StreamingSource*>(object_ptr);

		MtMutexGuard lock{mt_3d_objects_mutex_};

		return source.user_data_array_[index];
	}

	void api_stop_3d_sample(
		LH3DSAMPLE sample_handle)
	{
		if (!sample_handle)
		{
			return;
		}

		auto& source = *static_cast<StreamingSource*>(sample_handle);

		{
			MtMutexGuard lock{mt_3d_objects_mutex_};

			source.pause();
		}

		mt_notify_stream();
	}

	void api_start_3d_sample(
		LH3DSAMPLE sample_handle)
	{
		if (!sample_handle)
		{
			return;
		}

		auto& source = *static_cast<StreamingSource*>(sample_handle);

		{
			MtMutexGuard lock{mt_3d_objects_mutex_};

			source.stop();
			source.resume();
		}

		mt_notify_stream();
	}

	void api_resume_3d_sample(
		LH3DSAMPLE sample_handle)
	{
		if (!sample_handle)
		{
			return;
		}

		auto& source = *static_cast<StreamingSource*>(sample_handle);

		{
			MtMutexGuard lock{mt_3d_objects_mutex_};

			source.resume();
		}

		mt_notify_stream();
	}

	void api_end_3d_sample(
		LH3DSAMPLE sample_handle)
	{
		if (!sample_handle)
		{
			return;
		}

		auto& source = *static_cast<StreamingSource*>(sample_handle);

		{
			MtMutexGuard lock{mt_3d_objects_mutex_};

			source.stop();
		}

		mt_notify_stream();
	}

	sint32 api_init_3d_sample_from_address(
		LH3DSAMPLE sample_handle,
		const void* storage_ptr,
		const uint32 storage_size,
		const ul::WaveFormatEx& wave_format,
		const sint32 playback_rate,
		const LTSOUNDFILTERDATA* filter_data_ptr)
	{
		static_cast<void>(filter_data_ptr);

		if (!sample_handle)
		{
			return false;
		}

		auto& source = *static_cast<StreamingSource*>(sample_handle);

		if (source.is_failed())
		{
			return false;
		}

		auto open_parameters = StreamingSource::OpenParameters{};

		open_parameters.is_memory_ = true;
		open_parameters.memory_ptr_ = storage_ptr;
		open_parameters.memory_size_ = storage_size;
		open_parameters.memory_wave_format_ = wave_format;
		open_parameters.playback_rate_ = playback_rate;
		open_parameters.oal_has_effect_slot_ = oal_is_supports_eax20_filter_;
		open_parameters.oal_has_effect_slot_ = oal_effect_slot_;

		MtMutexGuard lock{mt_samples_mutex_};

		if (!source.open(open_parameters))
		{
			return false;
		}

		mt_notify_stream();

		return true;
	}

	sint32 api_init_3d_sample_from_file(
		LH3DSAMPLE sample_handle,
		const void* storage_ptr,
		const sint32 block,
		const sint32 playback_rate,
		const LTSOUNDFILTERDATA* filter_data_ptr)
	{
		static_cast<void>(block);
		static_cast<void>(filter_data_ptr);

		if (!sample_handle)
		{
			return false;
		}

		auto& source = *static_cast<StreamingSource*>(sample_handle);

		if (source.is_failed())
		{
			return false;
		}

		auto open_parameters = StreamingSource::OpenParameters{};

		open_parameters.is_mapped_file_ = true;
		open_parameters.mapped_decoder_ = &audio_decoder_;
		open_parameters.mapped_storage_ptr = storage_ptr;
		open_parameters.playback_rate_ = playback_rate;
		open_parameters.oal_has_effect_slot_ = oal_is_supports_eax20_filter_;
		open_parameters.oal_has_effect_slot_ = oal_effect_slot_;

		MtMutexGuard lock{mt_samples_mutex_};

		if (!source.open(open_parameters))
		{
			return false;
		}

		mt_notify_stream();

		return true;
	}

	sint32 api_get_3d_sample_volume(
		LH3DSAMPLE sample_handle)
	{
		if (!sample_handle)
		{
			return {};
		}

		auto& source = *static_cast<StreamingSource*>(sample_handle);

		MtMutexGuard lock{mt_3d_objects_mutex_};

		return source.get_3d_volume();
	}

	uint32 api_get_3d_sample_status(
		LH3DSAMPLE sample_handle)
	{
		if (!sample_handle)
		{
			return LS_STOPPED;
		}

		auto& source = *static_cast<StreamingSource*>(sample_handle);

		MtMutexGuard lock{mt_3d_objects_mutex_};

		return source.is_playing() ? LS_PLAYING : LS_STOPPED;
	}

	void api_set_3d_sample_ms_position(
		LHSAMPLE sample_handle,
		const sint32 milliseconds)
	{
		if (!sample_handle)
		{
			return;
		}

		auto& source = *static_cast<StreamingSource*>(sample_handle);

		MtMutexGuard lock{mt_3d_objects_mutex_};

		source.set_ms_position(milliseconds);
	}

	void api_set_3d_sample_loop_block(
		LH3DSAMPLE sample_handle,
		const sint32 loop_begin_offset,
		const sint32 loop_end_offset,
		const bool is_enable)
	{
		if (!sample_handle)
		{
			return;
		}

		auto& source = *static_cast<StreamingSource*>(sample_handle);

		MtMutexGuard lock{mt_3d_objects_mutex_};

		source.set_loop_block(loop_begin_offset, loop_end_offset, is_enable);
	}

	void api_set_3d_sample_loop(
		LH3DSAMPLE sample_handle,
		const bool is_enable)
	{
		if (!sample_handle)
		{
			return;
		}

		auto& source = *static_cast<StreamingSource*>(sample_handle);

		MtMutexGuard lock{mt_3d_objects_mutex_};

		source.set_loop(is_enable);
	}

	bool api_set_eax20_filter(
		const bool is_enable,
		const LTSOUNDFILTERDATA& filter_data)
	{
		if (filter_data.uiFilterType != FilterReverb || !oal_is_supports_eax20_filter_)
		{
			return false;
		}

		const auto& lt_reverb = *reinterpret_cast<const LTFILTERREVERB*>(filter_data.pSoundFilter);

		lt_reverb_to_efx_reverb(lt_reverb, oal_efx_eax_reverb_properties_);

		oal_clear_error();

		oal_update_reverb_effect();

		const auto oal_effect = (is_enable ? oal_effect_ : oal_null_effect_);
		alAuxiliaryEffectSloti_(oal_effect_slot_, AL_EFFECTSLOT_EFFECT, oal_effect);

		if (!oal_is_succeed())
		{
			return false;
		}

		return true;
	}

	bool api_supports_eax20_filter() const
	{
		return oal_is_supports_eax20_filter_;
	}

	bool api_set_eax20_buffer_settings(
		LHSAMPLE sample_handle,
		const LTSOUNDFILTERDATA& filter_data)
	{
		if (!sample_handle)
		{
			return false;
		}

		static_cast<void>(filter_data);

		return true;
	}

	void api_handle_focus_lost(
		const bool is_focus_lost)
	{
		listener_3d_uptr_->mute_3d_listener(is_focus_lost);
	}

	//
	// =========================================================================
	// API
	// =========================================================================


	String error_message_;
	bool is_eax20_filters_defined_;

	ALCdevice* oal_device_;
	ALCcontext* oal_context_;

	OalVersion oal_version_;
	String oal_version_string_;
	String oal_vendor_string_;
	String oal_renderer_string_;
	ExtensionsStrings oal_extentions_strings_;
	int oal_context_sample_rate_;

	OalVersion oal_efx_version_;
	bool oal_has_efx_;
	bool oal_has_chorus_effect_;
	bool oal_has_compressor_effect_;
	bool oal_has_distortion_effect_;
	bool oal_has_echo_effect_;
	bool oal_has_flange_effect_;
	bool oal_has_equalizer_effect_;
	bool oal_has_reverb_effect_;
	bool oal_has_eax_reverb_effect_;
	int oal_max_aux_sends_;

	bool oal_is_supports_eax20_filter_;
	ALuint oal_effect_;
	ALuint oal_null_effect_;
	ALuint oal_effect_slot_;
	EFXEAXREVERBPROPERTIES oal_efx_eax_reverb_properties_;

	LPALGENEFFECTS alGenEffects_;
	LPALDELETEEFFECTS alDeleteEffects_;
	LPALISEFFECT alIsEffect_;
	LPALEFFECTI alEffecti_;
	LPALEFFECTIV alEffectiv_;
	LPALEFFECTF alEffectf_;
	LPALEFFECTFV alEffectfv_;
	LPALGETEFFECTI alGetEffecti_;
	LPALGETEFFECTIV alGetEffectiv_;
	LPALGETEFFECTF alGetEffectf_;
	LPALGETEFFECTFV alGetEffectfv_;
	LPALGENFILTERS alGenFilters_;
	LPALDELETEFILTERS alDeleteFilters_;
	LPALISFILTER alIsFilter_;
	LPALFILTERI alFilteri_;
	LPALFILTERIV alFilteriv_;
	LPALFILTERF alFilterf_;
	LPALFILTERFV alFilterfv_;
	LPALGETFILTERI alGetFilteri_;
	LPALGETFILTERIV alGetFilteriv_;
	LPALGETFILTERF alGetFilterf_;
	LPALGETFILTERFV alGetFilterfv_;
	LPALGENAUXILIARYEFFECTSLOTS alGenAuxiliaryEffectSlots_;
	LPALDELETEAUXILIARYEFFECTSLOTS alDeleteAuxiliaryEffectSlots_;
	LPALISAUXILIARYEFFECTSLOT alIsAuxiliaryEffectSlot_;
	LPALAUXILIARYEFFECTSLOTI alAuxiliaryEffectSloti_;
	LPALAUXILIARYEFFECTSLOTIV alAuxiliaryEffectSlotiv_;
	LPALAUXILIARYEFFECTSLOTF alAuxiliaryEffectSlotf_;
	LPALAUXILIARYEFFECTSLOTFV alAuxiliaryEffectSlotfv_;
	LPALGETAUXILIARYEFFECTSLOTI alGetAuxiliaryEffectSloti_;
	LPALGETAUXILIARYEFFECTSLOTIV alGetAuxiliaryEffectSlotiv_;
	LPALGETAUXILIARYEFFECTSLOTF alGetAuxiliaryEffectSlotf_;
	LPALGETAUXILIARYEFFECTSLOTFV alGetAuxiliaryEffectSlotfv_;


	ltjs::AudioDecoder audio_decoder_;

	ClockTs clock_base_;
	sint32 master_volume_;
	float oal_master_gain_;
	bool is_mute_;

	Sources samples_;
	MtMutex mt_samples_mutex_;
	OpenSources mt_open_samples_;

	StreamingSourceUPtr listener_3d_uptr_;
	Sources objects_3d_;
	MtMutex mt_3d_objects_mutex_;
	OpenSources mt_open_3d_objects_;

	Sources streams_;
	MtMutex mt_streams_mutex_;
	OpenSources mt_open_streams_;

	MtThread mt_sound_thread_;
	MtCondVar mt_sound_cv_;
	MtMutex mt_sound_cv_mutex_;

	bool mt_is_stop_sound_worker_;
	bool mt_sound_cv_flag_;
}; // OalSoundSys::Impl


const float OalSoundSys::Impl::min_doppler_factor = 0.0F;
const float OalSoundSys::Impl::max_doppler_factor = 10.0F;
const float OalSoundSys::Impl::default_3d_doppler_factor = 1.0F;

const float OalSoundSys::Impl::min_min_distance = 0.0F;
const float OalSoundSys::Impl::max_min_distance = std::numeric_limits<float>::max();
const float OalSoundSys::Impl::default_3d_min_distance = 1.0F;

const float OalSoundSys::Impl::min_max_distance = 0.0F;
const float OalSoundSys::Impl::max_max_distance = std::numeric_limits<float>::max();
const float OalSoundSys::Impl::default_3d_max_distance = 1'000'000'000.0F;

const float OalSoundSys::Impl::min_gain = 0.0F;
const float OalSoundSys::Impl::max_gain = std::numeric_limits<float>::max();
const float OalSoundSys::Impl::default_gain = 1.0F;

const OalSoundSys::Impl::Vector3d OalSoundSys::Impl::default_3d_position = {};
const OalSoundSys::Impl::Vector3d OalSoundSys::Impl::default_3d_velocity = {};
const OalSoundSys::Impl::Vector3d OalSoundSys::Impl::default_3d_direction = {};
const OalSoundSys::Impl::Orientation3d OalSoundSys::Impl::default_3d_orientation = {{0.0F, 0.0F, 1.0F}, {0.0F, 1.0F, 0.0}};

const OalSoundSys::Impl::EfxReverbPresets OalSoundSys::Impl::efx_reverb_presets =
{{
	EFX_REVERB_PRESET_GENERIC,
	EFX_REVERB_PRESET_PADDEDCELL,
	EFX_REVERB_PRESET_ROOM,
	EFX_REVERB_PRESET_BATHROOM,
	EFX_REVERB_PRESET_LIVINGROOM,
	EFX_REVERB_PRESET_STONEROOM,
	EFX_REVERB_PRESET_AUDITORIUM,
	EFX_REVERB_PRESET_CONCERTHALL,
	EFX_REVERB_PRESET_CAVE,
	EFX_REVERB_PRESET_ARENA,
	EFX_REVERB_PRESET_HANGAR,
	EFX_REVERB_PRESET_CARPETEDHALLWAY,
	EFX_REVERB_PRESET_HALLWAY,
	EFX_REVERB_PRESET_STONECORRIDOR,
	EFX_REVERB_PRESET_ALLEY,
	EFX_REVERB_PRESET_FOREST,
	EFX_REVERB_PRESET_CITY,
	EFX_REVERB_PRESET_MOUNTAINS,
	EFX_REVERB_PRESET_QUARRY,
	EFX_REVERB_PRESET_PLAIN,
	EFX_REVERB_PRESET_PARKINGLOT,
	EFX_REVERB_PRESET_SEWERPIPE,
	EFX_REVERB_PRESET_UNDERWATER,
	EFX_REVERB_PRESET_DRUGGED,
	EFX_REVERB_PRESET_DIZZY,
	EFX_REVERB_PRESET_PSYCHOTIC,
}}; // efx_reverb_presets


OalSoundSys::OalSoundSys()
	:
	pimpl_{new Impl{}}
{
}

OalSoundSys::OalSoundSys(
	OalSoundSys&& that)
	:
	pimpl_{std::move(that.pimpl_)}
{
}

OalSoundSys::~OalSoundSys()
{
}

bool OalSoundSys::Init()
{
	ltjs::AudioDecoder::initialize_current_thread();
	ltjs::AudioUtils::initialize();

	return true;
}

void OalSoundSys::Term()
{
}

void* OalSoundSys::GetDDInterface(
	const uint dd_interface_id)
{
	static_cast<void>(dd_interface_id);

	return {};
}

void OalSoundSys::Lock()
{
}

void OalSoundSys::Unlock()
{
}

sint32 OalSoundSys::Startup()
{
	return pimpl_->api_startup();
}

void OalSoundSys::Shutdown()
{
	pimpl_->api_shutdown();
}

uint32 OalSoundSys::MsCount()
{
	return pimpl_->api_ms_count();
}

sint32 OalSoundSys::SetPreference(
	const uint32 index,
	const sint32 value)
{
	static_cast<void>(index);
	static_cast<void>(value);

	return LS_ERROR;
}

sint32 OalSoundSys::GetPreference(
	const uint32 index)
{
	static_cast<void>(index);

	return {};
}

void OalSoundSys::MemFreeLock(
	void* storage_ptr)
{
	ltjs::AudioUtils::deallocate(storage_ptr);
}

void* OalSoundSys::MemAllocLock(
	const uint32 storage_size)
{
	return ltjs::AudioUtils::allocate(storage_size);
}

const char* OalSoundSys::LastError()
{
	return pimpl_->api_last_error();
}

sint32 OalSoundSys::WaveOutOpen(
	LHDIGDRIVER& driver,
	PHWAVEOUT& wave_out,
	const sint32 device_id,
	const ul::WaveFormatEx& wave_format)
{
	return pimpl_->api_wave_out_open(driver, wave_out, device_id, wave_format);
}

void OalSoundSys::WaveOutClose(
	LHDIGDRIVER driver_ptr)
{
	pimpl_->api_wave_out_close(driver_ptr);
}

void OalSoundSys::SetDigitalMasterVolume(
	LHDIGDRIVER driver_ptr,
	const sint32 master_volume)
{
	pimpl_->api_set_digital_master_volume(driver_ptr, master_volume);
}

sint32 OalSoundSys::GetDigitalMasterVolume(
	LHDIGDRIVER driver_ptr)
{
	return pimpl_->api_get_digital_master_volume(driver_ptr);
}

sint32 OalSoundSys::DigitalHandleRelease(
	LHDIGDRIVER driver_ptr)
{
	static_cast<void>(driver_ptr);

	return {};
}

sint32 OalSoundSys::DigitalHandleReacquire(
	LHDIGDRIVER driver_ptr)
{
	static_cast<void>(driver_ptr);

	return {};
}

#ifdef USE_EAX20_HARDWARE_FILTERS
bool OalSoundSys::SetEAX20Filter(
	const bool is_enable,
	const LTSOUNDFILTERDATA& filter_data)
{
	return pimpl_->api_set_eax20_filter(is_enable, filter_data);
}

bool OalSoundSys::SupportsEAX20Filter()
{
	return pimpl_->api_supports_eax20_filter();
}

bool OalSoundSys::SetEAX20BufferSettings(
	LHSAMPLE sample_handle,
	const LTSOUNDFILTERDATA& filter_data)
{
	return pimpl_->api_set_eax20_buffer_settings(sample_handle, filter_data);
}
#endif // USE_EAX20_HARDWARE_FILTERS

void OalSoundSys::Set3DProviderMinBuffers(
	const uint32 buffer_count)
{
	static_cast<void>(buffer_count);
}

sint32 OalSoundSys::Open3DProvider(
	LHPROVIDER provider_id)
{
	switch (provider_id)
	{
	case SOUND3DPROVIDERID_DS3D_SOFTWARE:
	case SOUND3DPROVIDERID_DS3D_HARDWARE:
	case SOUND3DPROVIDERID_DS3D_DEFAULT:
		return true;

	default:
		return false;
	}
}

void OalSoundSys::Close3DProvider(
	LHPROVIDER provider_id)
{
	static_cast<void>(provider_id);
}

void OalSoundSys::Set3DProviderPreference(
	LHPROVIDER provider_id,
	const char* name,
	const void* value)
{
	static_cast<void>(provider_id);
	static_cast<void>(name);
	static_cast<void>(value);
}

void OalSoundSys::Get3DProviderAttribute(
	LHPROVIDER provider_id,
	const char* name,
	void* value)
{
	pimpl_->api_get_3d_provider_attribute(provider_id, name, value);
}

sint32 OalSoundSys::Enumerate3DProviders(
	LHPROENUM& index,
	LHPROVIDER& id,
	const char*& name)
{
	const auto current_index = index++;

	id = 0;
	name = nullptr;

	if (current_index < 0 || current_index > 0)
	{
		return false;
	}

	id = SOUND3DPROVIDERID_DS3D_HARDWARE;
	name = "OpenAL";

	return true;
}

LH3DPOBJECT OalSoundSys::Open3DListener(
	LHPROVIDER provider_id)
{
	return pimpl_->api_open_3d_listener(provider_id);
}

void OalSoundSys::Close3DListener(
	LH3DPOBJECT listener_ptr)
{
	pimpl_->api_close_3d_listener(listener_ptr);
}

void OalSoundSys::SetListenerDoppler(
	LH3DPOBJECT listener_ptr,
	const float doppler_factor)
{
	pimpl_->api_set_listener_doppler(listener_ptr, doppler_factor);
}

void OalSoundSys::CommitDeferred()
{
}

void OalSoundSys::Set3DPosition(
	LH3DPOBJECT object_ptr,
	const float x,
	const float y,
	const float z)
{
	pimpl_->api_set_3d_position(object_ptr, x, y, z);
}

void OalSoundSys::Set3DVelocityVector(
	LH3DPOBJECT object_ptr,
	const float dx_per_s,
	const float dy_per_s,
	const float dz_per_s)
{
	pimpl_->api_set_3d_velocity_vector(object_ptr, dx_per_s, dy_per_s, dz_per_s);
}

void OalSoundSys::Set3DOrientation(
	LH3DPOBJECT object_ptr,
	const float x_face,
	const float y_face,
	const float z_face,
	const float x_up,
	const float y_up,
	const float z_up)
{
	pimpl_->api_set_3d_orientation(object_ptr, x_face, y_face, z_face, x_up, y_up, z_up);
}

void OalSoundSys::Set3DUserData(
	LH3DPOBJECT object_ptr,
	const uint32 index,
	const sint32 value)
{
	pimpl_->api_set_3d_user_data(object_ptr, index, value);
}

void OalSoundSys::Get3DPosition(
	LH3DPOBJECT object_ptr,
	float& x,
	float& y,
	float& z)
{
	pimpl_->api_get_3d_position(object_ptr, x, y, z);
}

void OalSoundSys::Get3DVelocity(
	LH3DPOBJECT object_ptr,
	float& dx_per_ms,
	float& dy_per_ms,
	float& dz_per_ms)
{
	pimpl_->api_get_3d_velocity(object_ptr, dx_per_ms, dy_per_ms, dz_per_ms);
}

void OalSoundSys::Get3DOrientation(
	LH3DPOBJECT object_ptr,
	float& x_face,
	float& y_face,
	float& z_face,
	float& x_up,
	float& y_up,
	float& z_up)
{
	pimpl_->api_get_3d_orientation(object_ptr, x_face, y_face, z_face, x_up, y_up, z_up);
}

sint32 OalSoundSys::Get3DUserData(
	LH3DPOBJECT object_ptr,
	const uint32 index)
{
	return pimpl_->api_get_3d_user_data(object_ptr, index);
}

LH3DSAMPLE OalSoundSys::Allocate3DSampleHandle(
	LHPROVIDER driver_id)
{
	return pimpl_->api_allocate_3d_sample_handle(driver_id);
}

void OalSoundSys::Release3DSampleHandle(
	LH3DSAMPLE sample_handle)
{
	return pimpl_->api_release_3d_sample_handle(sample_handle);
}

void OalSoundSys::Stop3DSample(
	LH3DSAMPLE sample_handle)
{
	pimpl_->api_stop_3d_sample(sample_handle);
}

void OalSoundSys::Start3DSample(
	LH3DSAMPLE sample_handle)
{
	pimpl_->api_start_3d_sample(sample_handle);
}

void OalSoundSys::Resume3DSample(
	LH3DSAMPLE sample_handle)
{
	pimpl_->api_resume_3d_sample(sample_handle);
}

void OalSoundSys::End3DSample(
	LH3DSAMPLE sample_handle)
{
	pimpl_->api_end_3d_sample(sample_handle);
}

sint32 OalSoundSys::Init3DSampleFromAddress(
	LH3DSAMPLE sample_handle,
	const void* ptr,
	const uint32 length,
	const ul::WaveFormatEx& wave_format,
	const sint32 playback_rate,
	const LTSOUNDFILTERDATA* filter_data_ptr)
{
	return pimpl_->api_init_3d_sample_from_address(sample_handle, ptr, length, wave_format, playback_rate, filter_data_ptr);
}

sint32 OalSoundSys::Init3DSampleFromFile(
	LH3DSAMPLE sample_handle,
	const void* storage_ptr,
	const sint32 block,
	const sint32 playback_rate,
	const LTSOUNDFILTERDATA* filter_data_ptr)
{
	return pimpl_->api_init_3d_sample_from_file(sample_handle, storage_ptr, block, playback_rate, filter_data_ptr);
}

sint32 OalSoundSys::Get3DSampleVolume(
	LH3DSAMPLE sample_handle)
{
	return pimpl_->api_get_3d_sample_volume(sample_handle);
}

void OalSoundSys::Set3DSampleVolume(
	LH3DSAMPLE sample_handle,
	const sint32 volume)
{
	pimpl_->api_set_3d_sample_volume(sample_handle, volume);
}

uint32 OalSoundSys::Get3DSampleStatus(
	LH3DSAMPLE sample_handle)
{
	return pimpl_->api_get_3d_sample_status(sample_handle);
}

void OalSoundSys::Set3DSampleMsPosition(
	LHSAMPLE sample_handle,
	const sint32 milliseconds)
{
	pimpl_->api_set_3d_sample_ms_position(sample_handle, milliseconds);
}

sint32 OalSoundSys::Set3DSampleInfo(
	LH3DSAMPLE sample_handle,
	const LTSOUNDINFO& sound_info)
{
	static_cast<void>(sample_handle);
	static_cast<void>(sound_info);

	return {};
}

void OalSoundSys::Set3DSampleDistances(
	LH3DSAMPLE sample_handle,
	const float max_distance,
	const float min_distance)
{
	pimpl_->api_set_3d_sample_distances(sample_handle, max_distance, min_distance);
}

void OalSoundSys::Set3DSamplePreference(
	LH3DSAMPLE sample_handle,
	const char* name,
	const void* value)
{
	static_cast<void>(sample_handle);
	static_cast<void>(name);
	static_cast<void>(value);
}

void OalSoundSys::Set3DSampleLoopBlock(
	LH3DSAMPLE sample_handle,
	const sint32 loop_begin_offset,
	const sint32 loop_end_offset,
	const bool is_enable)
{
	pimpl_->api_set_3d_sample_loop_block(
		sample_handle, loop_begin_offset, loop_end_offset, is_enable);
}

void OalSoundSys::Set3DSampleLoop(
	LH3DSAMPLE sample_handle,
	const bool is_enable)
{
	pimpl_->api_set_3d_sample_loop(sample_handle, is_enable);
}

void OalSoundSys::Set3DSampleObstruction(
	LH3DSAMPLE sample_handle,
	const float obstruction)
{
	static_cast<void>(sample_handle);
	static_cast<void>(obstruction);
}

float OalSoundSys::Get3DSampleObstruction(
	LH3DSAMPLE sample_handle)
{
	static_cast<void>(sample_handle);

	return {};
}

void OalSoundSys::Set3DSampleOcclusion(
	LH3DSAMPLE sample_handle,
	const float occlusion)
{
	static_cast<void>(sample_handle);
	static_cast<void>(occlusion);
}

float OalSoundSys::Get3DSampleOcclusion(
	LH3DSAMPLE sample_handle)
{
	static_cast<void>(sample_handle);

	return {};
}

LHSAMPLE OalSoundSys::AllocateSampleHandle(
	LHDIGDRIVER driver_ptr)
{
	return pimpl_->api_allocate_sample_handle(driver_ptr);
}

void OalSoundSys::ReleaseSampleHandle(
	LHSAMPLE sample_handle)
{
	pimpl_->api_release_sample_handle(sample_handle);
}

void OalSoundSys::InitSample(
	LHSAMPLE sample_handle)
{
	static_cast<void>(sample_handle);
}

void OalSoundSys::StopSample(
	LHSAMPLE sample_handle)
{
	pimpl_->api_stop_sample(sample_handle);
}

void OalSoundSys::StartSample(
	LHSAMPLE sample_handle)
{
	pimpl_->api_start_sample(sample_handle);
}

void OalSoundSys::ResumeSample(
	LHSAMPLE sample_handle)
{
	pimpl_->api_resume_sample(sample_handle);
}

void OalSoundSys::EndSample(
	LHSAMPLE sample_handle)
{
	pimpl_->api_end_sample(sample_handle);
}

void OalSoundSys::SetSampleVolume(
	LHSAMPLE sample_handle,
	const sint32 volume)
{
	pimpl_->api_set_sample_volume(sample_handle, volume);
}

void OalSoundSys::SetSamplePan(
	LHSAMPLE sample_handle,
	const sint32 pan)
{
	pimpl_->api_set_sample_pan(sample_handle, pan);
}

sint32 OalSoundSys::GetSampleVolume(
	LHSAMPLE sample_handle)
{
	return pimpl_->api_get_sample_volume(sample_handle);
}

sint32 OalSoundSys::GetSamplePan(
	LHSAMPLE sample_handle)
{
	return pimpl_->api_get_sample_pan(sample_handle);
}

void OalSoundSys::SetSampleUserData(
	LHSAMPLE sample_handle,
	const uint32 index,
	const sint32 value)
{
	pimpl_->api_set_sample_user_data(sample_handle, index, value);
}

void OalSoundSys::GetDirectSoundInfo(
	LHSAMPLE sample_handle,
	PTDIRECTSOUND& ds_instance,
	PTDIRECTSOUNDBUFFER& ds_buffer)
{
	static_cast<void>(sample_handle);

	ds_instance = nullptr;
	ds_buffer = nullptr;
}

void OalSoundSys::SetSampleReverb(
	LHSAMPLE sample_handle,
	const float reverb_level,
	const float reverb_reflect_time,
	const float reverb_decay_time)
{
	static_cast<void>(sample_handle);
	static_cast<void>(reverb_level);
	static_cast<void>(reverb_reflect_time);
	static_cast<void>(reverb_decay_time);
}

sint32 OalSoundSys::InitSampleFromAddress(
	LHSAMPLE sample_handle,
	const void* storage_ptr,
	const uint32 storage_size,
	const ul::WaveFormatEx& wave_format,
	const sint32 playback_rate,
	const LTSOUNDFILTERDATA* filter_data_ptr)
{
	return pimpl_->api_init_sample_from_address(
		sample_handle, storage_ptr, storage_size, wave_format, playback_rate, filter_data_ptr);
}

sint32 OalSoundSys::InitSampleFromFile(
	LHSAMPLE sample_handle,
	const void* storage_ptr,
	const sint32 block,
	const sint32 playback_rate,
	const LTSOUNDFILTERDATA* filter_data_ptr)
{
	return pimpl_->api_init_sample_from_file(
		sample_handle, storage_ptr, block, playback_rate, filter_data_ptr);
}

void OalSoundSys::SetSampleLoopBlock(
	LHSAMPLE sample_handle,
	const sint32 loop_begin_offset,
	const sint32 loop_end_offset,
	const bool is_enable)
{
	pimpl_->api_set_sample_loop_block(
		sample_handle, loop_begin_offset, loop_end_offset, is_enable);
}

void OalSoundSys::SetSampleLoop(
	LHSAMPLE sample_handle,
	const bool is_enable)
{
	pimpl_->api_set_sample_loop(sample_handle, is_enable);
}

void OalSoundSys::SetSampleMsPosition(
	LHSAMPLE sample_handle,
	const sint32 milliseconds)
{
	pimpl_->api_set_sample_ms_position(sample_handle, milliseconds);
}

sint32 OalSoundSys::GetSampleUserData(
	LHSAMPLE sample_handle,
	const uint32 index)
{
	return pimpl_->api_get_sample_user_data(sample_handle, index);
}

uint32 OalSoundSys::GetSampleStatus(
	LHSAMPLE sample_handle)
{
	return pimpl_->api_get_sample_status(sample_handle);
}

LHSTREAM OalSoundSys::OpenStream(
	const char* file_name,
	const uint32 file_offset,
	LHDIGDRIVER driver_ptr,
	const char* storage_ptr,
	const sint32 storage_size)
{
	return pimpl_->api_open_stream(
		file_name, file_offset, driver_ptr, storage_ptr, storage_size);
}

void OalSoundSys::SetStreamLoop(
	LHSTREAM stream_ptr,
	const bool is_enable)
{
	pimpl_->set_set_stream_loop(stream_ptr, is_enable);
}

void OalSoundSys::SetStreamPlaybackRate(
	LHSTREAM stream_ptr,
	const sint32 rate)
{
	static_cast<void>(stream_ptr);
	static_cast<void>(rate);
}

void OalSoundSys::SetStreamMsPosition(
	LHSTREAM stream_ptr,
	const sint32 milliseconds)
{
	pimpl_->api_set_stream_ms_position(stream_ptr, milliseconds);
}

void OalSoundSys::SetStreamUserData(
	LHSTREAM stream_ptr,
	const uint32 index,
	const sint32 value)
{
	pimpl_->api_set_stream_user_data(stream_ptr, index,value);
}

sint32 OalSoundSys::GetStreamUserData(
	LHSTREAM stream_ptr,
	const uint32 index)
{
	return pimpl_->api_get_stream_user_data(stream_ptr, index);
}

void OalSoundSys::CloseStream(
	LHSTREAM stream_ptr)
{
	pimpl_->api_close_stream(stream_ptr);
}

void OalSoundSys::StartStream(
	LHSTREAM stream_ptr)
{
	pimpl_->api_start_stream(stream_ptr);
}

void OalSoundSys::PauseStream(
	LHSTREAM stream_ptr,
	const sint32 is_enable)
{
	pimpl_->api_pause_stream(stream_ptr, is_enable);
}

void OalSoundSys::ResetStream(
	LHSTREAM stream_ptr)
{
	static_cast<void>(stream_ptr);
}

void OalSoundSys::SetStreamVolume(
	LHSTREAM stream_ptr,
	const sint32 volume)
{
	pimpl_->api_set_stream_volume(stream_ptr, volume);
}

void OalSoundSys::SetStreamPan(
	LHSTREAM stream_ptr,
	const sint32 pan)
{
	pimpl_->api_set_stream_pan(stream_ptr, pan);
}

sint32 OalSoundSys::GetStreamVolume(
	LHSTREAM stream_ptr)
{
	return pimpl_->api_get_stream_volume(stream_ptr);
}

sint32 OalSoundSys::GetStreamPan(
	LHSTREAM stream_ptr)
{
	return pimpl_->api_get_stream_pan(stream_ptr);
}

uint32 OalSoundSys::GetStreamStatus(
	LHSTREAM stream_ptr)
{
	return pimpl_->api_get_stream_status(stream_ptr);
}

sint32 OalSoundSys::GetStreamBufferParam(
	LHSTREAM stream_ptr,
	const uint32 index)
{
	static_cast<void>(stream_ptr);
	static_cast<void>(index);

	return {};
}

void OalSoundSys::ClearStreamBuffer(
	LHSTREAM stream_ptr,
	const bool is_clear_data_queue)
{
	static_cast<void>(stream_ptr);
	static_cast<void>(is_clear_data_queue);
}

sint32 OalSoundSys::DecompressADPCM(
	const LTSOUNDINFO& sound_info,
	void*& dst_data,
	uint32& dst_size)
{
	static_cast<void>(sound_info);
	static_cast<void>(dst_data);
	static_cast<void>(dst_size);

	return {};
}

sint32 OalSoundSys::DecompressASI(
	const void* src_data_ptr,
	const uint32 src_data_size,
	const char* file_name_ext,
	void*& dst_wav_ptr,
	uint32& dst_wav_size,
	LTLENGTHYCB callback)
{
	return pimpl_->api_decode_mp3(src_data_ptr, src_data_size, file_name_ext, dst_wav_ptr, dst_wav_size, callback);
}

uint32 OalSoundSys::GetThreadedSoundTicks()
{
	return {};
}

bool OalSoundSys::HasOnBoardMemory()
{
	return {};
}

void OalSoundSys::handle_focus_lost(
	const bool is_focus_lost)
{
	pimpl_->api_handle_focus_lost(is_focus_lost);
}

OalSoundSys& OalSoundSys::get_singleton()
{
	static auto singleton = OalSoundSys{};
	return singleton;
}


extern "C"
{
	__declspec(dllexport) char* SoundSysDesc();
	__declspec(dllexport) ILTSoundSys* SoundSysMake();
}

char* SoundSysDesc()
{
	static char* description = const_cast<char*>("OpenAL");
	return description;
}

ILTSoundSys* SoundSysMake()
{
	return &OalSoundSys::get_singleton();
}
