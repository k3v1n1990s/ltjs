#include "precompile.h"
#include "ltjs_ogl_renderer.h"
#include <cassert>
#include <algorithm>
#include <array>
#include <bitset>
#include <iterator>
#include <list>
#include <memory>
#include <numeric>
#include <sstream>
#include <string>
#include <utility>
#include <vector>
#include "glad.h"


// --------------------------------------------------------------------------
// GL_CLIP_VOLUME_CLIPPING_HINT_EXT
//

#ifndef GL_CLIP_VOLUME_CLIPPING_HINT_EXT
#define GL_CLIP_VOLUME_CLIPPING_HINT_EXT (0x80F0)
#endif // GL_CLIP_VOLUME_CLIPPING_HINT_EXT

//
// GL_CLIP_VOLUME_CLIPPING_HINT_EXT
// --------------------------------------------------------------------------


// --------------------------------------------------------------------------
// GL_ARB_clip_control
//

#ifndef GL_CLIP_ORIGIN
#define GL_CLIP_ORIGIN (0x935C)
#endif // GL_CLIP_ORIGIN

#ifndef GL_CLIP_DEPTH_MODE
#define GL_CLIP_DEPTH_MODE (0x935D)
#endif // GL_CLIP_DEPTH_MODE

#ifndef GL_NEGATIVE_ONE_TO_ONE
#define GL_NEGATIVE_ONE_TO_ONE (0x935E)
#endif // GL_NEGATIVE_ONE_TO_ONE

#ifndef GL_ZERO_TO_ONE
#define GL_ZERO_TO_ONE (0x935F)
#endif // GL_ZERO_TO_ONE

using PFNGLCLIPCONTROLPROC = void (APIENTRYP)(GLenum origin, GLenum depth);
static PFNGLCLIPCONTROLPROC glClipControl = nullptr;

//
// GL_ARB_clip_control
// --------------------------------------------------------------------------


namespace ltjs
{


namespace
{


constexpr std::uint32_t d3dfvf_xyz = 0x002;
constexpr std::uint32_t d3dfvf_xyzrhw = 0x004;

constexpr std::uint32_t d3dfvf_xyzb1 = 0x006;
constexpr std::uint32_t d3dfvf_xyzb2 = 0x008;
constexpr std::uint32_t d3dfvf_xyzb3 = 0x00A;

constexpr std::uint32_t d3dfvf_normal = 0x010;

constexpr std::uint32_t d3dfvf_diffuse = 0x040;

constexpr std::uint32_t d3dfvf_tex1 = 0x100;
constexpr std::uint32_t d3dfvf_tex2 = 0x200;
constexpr std::uint32_t d3dfvf_tex3 = 0x300;
constexpr std::uint32_t d3dfvf_tex4 = 0x400;

constexpr std::uint32_t d3dfvf_textureformat2 = 0; // Two floating point values
constexpr std::uint32_t d3dfvf_textureformat3 = 1; // Three floating point values
constexpr std::uint32_t d3dfvf_textureformat4 = 2; // Four floating point values


template<typename T>
T ogl_resolve_symbol(
	const char* const symbol_name)
{
	return reinterpret_cast<T>(::wglGetProcAddress(symbol_name));
}


constexpr std::uint32_t d3dfvf_texcoordsize2(
	const int index)
{
	return d3dfvf_textureformat2;
}

constexpr std::uint32_t d3dfvf_texcoordsize3(
	const int index)
{
	return d3dfvf_textureformat3 << ((index * 2) + 16);
}

constexpr std::uint32_t d3dfvf_texcoordsize4(
	const int index)
{
	return d3dfvf_textureformat4 << ((index * 2) + 16);
}

constexpr auto d3dfvf_valid_flags =
	d3dfvf_xyz |
	d3dfvf_xyzrhw |
	d3dfvf_xyzb1 |
	d3dfvf_xyzb2 |
	d3dfvf_xyzb3 |
	d3dfvf_normal |
	d3dfvf_diffuse |
	d3dfvf_tex1 |
	d3dfvf_tex2 |
	d3dfvf_tex4 |
	d3dfvf_texcoordsize3(0) |
	d3dfvf_texcoordsize3(1) |
	d3dfvf_texcoordsize4(1) |
	d3dfvf_texcoordsize4(2) |
	d3dfvf_texcoordsize4(3) |
	0;


GLenum usage_flags_to_ogl_usage(
	const OglRenderer::VertexArrayObject::UsageFlags usage_flags)
{
	if ((usage_flags & OglRenderer::VertexArrayObject::UsageFlags::is_dynamic) != 0)
	{
		if ((usage_flags & OglRenderer::VertexArrayObject::UsageFlags::is_readable) != 0)
		{
			return GL_DYNAMIC_COPY;
		}
		else
		{
			return GL_DYNAMIC_DRAW;
		}
	}
	else
	{
		if ((usage_flags & OglRenderer::VertexArrayObject::UsageFlags::is_readable) != 0)
		{
			return GL_STATIC_COPY;
		}
		else
		{
			return GL_STATIC_DRAW;
		}
	}
}


} // namespace


// ==========================================================================
// OglRendererImpl
//

class OglRendererImpl final :
	public OglRenderer
{
public:
	OglRendererImpl()
		:
		is_initialized_{},
		is_context_current_{},
		ogl_dc_{},
		ogl_rc_{},
		extensions_{},
		has_gl_arb_clip_control_{},
		has_gl_ext_clip_volume_hint_{},
		screen_width_{},
		screen_height_{},
		is_dirty_{},
		clear_color_r_{},
		clear_color_g_{},
		clear_color_b_{},
		clear_color_a_{},
		viewport_{},
		max_viewport_size_{},
		cull_mode_{},
		is_clipping_{},
		is_depth_enabled_{},
		is_depth_writable_{},
		depth_func_{},
		vertex_shader_{},
		fragment_shader_{},
		program_{},
		has_diffuse_{},
		u_has_diffuse_{},
		is_position_transformed_{},
		are_u_model_views_dirty_{},
		is_u_projection_dirty_{},
		world_matrices_{},
		view_matrix_{},
		projection_matrix_{},
		u_model_views_{},
		u_projection_{},
		current_vao_{},
		vertex_array_objects_{}
	{
	}

	~OglRendererImpl() override
	{
		assert(!is_initialized_);
	}


private:
	static constexpr auto max_world_matrices = 4;
	static constexpr auto max_texture_stages = 4;


	using Matrix4F = std::array<float, 16>;

	using WorldMatrix = Matrix4F;
	using WorldMatrices = std::array<WorldMatrix, max_world_matrices>;

	using ModelViewMatrixDirtyFlags = std::bitset<max_world_matrices>;

	using ViewMatrix = Matrix4F;

	using ModelViewMatrix = Matrix4F;

	using ProjectionMatrix = Matrix4F;

	using TextureMatrix = Matrix4F;
	using TextureMatrices = std::array<TextureMatrix, max_texture_stages>;

	using UModelViewMatrices = std::array<GLint, max_world_matrices>;
	using UTextureMatrices = std::array<GLint, max_texture_stages>;


	class VertexArrayObjectImpl final :
		public VertexArrayObject
	{
	public:
		VertexArrayObjectImpl(
			OglRendererImpl& impl);

		virtual ~VertexArrayObjectImpl();


	private:
		static constexpr auto index_element_size = 2;


		OglRendererImpl& impl_;

		bool is_initialized_;

		Fvf vertex_format_;
		UsageFlags vertex_usage_flags_;
		UsageFlags index_usage_flags_;
		int vertex_count_;
		int index_count_;

		GLuint ogl_vao_;
		GLuint ogl_vertex_buffer_;
		GLuint ogl_index_buffer_;


		bool do_initialize(
			const InitializeParam& param) override;

		bool initialize_internal(
			const InitializeParam& param);

		void do_uninitialize() override;

		void do_draw(
			const int index_base,
			const int vertex_base,
			const int triangle_count) override;

		void uninitialize_internal();
	}; // VertexArrayObjectImpl


	using VertexArrayObjectUPtr = std::unique_ptr<VertexArrayObjectImpl>;
	using VertexArrayObjectList = std::list<VertexArrayObjectUPtr>;

	using ViewportSize = std::array<int, 2>;
	using Extensions = std::vector<std::string>;

	enum class ShaderType
	{
		none,
		vertex,
		fragment,
	}; // ShaderType


	bool is_initialized_;
	bool is_context_current_;

	HDC ogl_dc_;
	HGLRC ogl_rc_;

	Extensions extensions_;

	bool has_gl_arb_clip_control_;
	bool has_gl_ext_clip_volume_hint_;

	int screen_width_;
	int screen_height_;

	bool is_dirty_;

	std::uint8_t clear_color_r_;
	std::uint8_t clear_color_g_;
	std::uint8_t clear_color_b_;
	std::uint8_t clear_color_a_;

	Viewport viewport_;
	ViewportSize max_viewport_size_;

	CullMode cull_mode_;

	bool is_clipping_;

	bool is_depth_enabled_;
	bool is_depth_writable_;
	DepthFunc depth_func_;

	GLuint vertex_shader_;
	GLuint fragment_shader_;
	GLuint program_;

	bool has_diffuse_;
	GLint u_has_diffuse_;

	bool is_position_transformed_;

	ModelViewMatrixDirtyFlags are_u_model_views_dirty_;
	bool is_u_projection_dirty_;

	WorldMatrices world_matrices_;
	ViewMatrix view_matrix_;
	ProjectionMatrix projection_matrix_;

	UModelViewMatrices u_model_views_;
	GLint u_projection_;

	VertexArrayObjectImpl* current_vao_;

	VertexArrayObjectList vertex_array_objects_;


	static int default_viewport_x;
	static int default_viewport_y;
	static float default_viewport_depth_min_z;
	static float default_viewport_depth_max_z;

	static std::uint8_t default_clear_color_r;
	static std::uint8_t default_clear_color_g;
	static std::uint8_t default_clear_color_b;
	static std::uint8_t default_clear_color_a;

	static const CullMode default_cull_mode;

	static const bool default_is_clipping;

	static const bool default_is_depth_enabled;
	static const bool default_is_depth_writable;
	static const DepthFunc default_depth_func;

	static const bool default_has_diffuse;

	static const Matrix4F identity_matrix;

	static const std::string vertex_shader_source;
	static const std::string fragment_shader_source;


	// ======================================================================
	// API
	//

	bool do_is_initialized() const override
	{
		return is_initialized_;
	}

	bool do_initialize(
		const int screen_width,
		const int screen_height) override
	{
		if (!initialize_internal(screen_width, screen_height))
		{
			uninitialize_internal();
			return false;
		}

		return true;
	}

	void do_uninitialize() override
	{
		uninitialize_internal();
	}

	void do_set_current_context(
		const bool is_current)
	{
		if (!is_initialized_)
		{
			assert(!"Invalid state.");
			return;
		}

		if (is_current == is_context_current_)
		{
			return;
		}

		set_current_context_internal(is_current);
	}

	void do_set_clear_color(
		const std::uint8_t r,
		const std::uint8_t g,
		const std::uint8_t b,
		const std::uint8_t a) override
	{
		if (!is_initialized_ || !is_context_current_)
		{
			assert(!"Invalid state.");
			return;
		}

		if (r == clear_color_r_ &&
			g == clear_color_g_ &&
			b == clear_color_b_ &&
			a == clear_color_a_)
		{
			return;
		}

		clear_color_r_ = r;
		clear_color_g_ = g;
		clear_color_b_ = b;
		clear_color_a_ = a;

		set_clear_color_internal();
	}

	void do_clear(
		const ClearFlags clear_flags) override
	{
		if (!is_initialized_ || !is_context_current_ || clear_flags == ClearFlags::none)
		{
			assert(!"Invalid state.");
			return;
		}

		auto ogl_clear_flags = GLbitfield{};

		if ((clear_flags & ClearFlags::color) != 0)
		{
			ogl_clear_flags |= GL_COLOR_BUFFER_BIT;
		}

		if ((clear_flags & ClearFlags::depth) != 0)
		{
			ogl_clear_flags |= GL_DEPTH_BUFFER_BIT;
		}

		if ((clear_flags & ClearFlags::stencil) != 0)
		{
			ogl_clear_flags |= GL_STENCIL_BUFFER_BIT;
		}

		::glClear(ogl_clear_flags);
		assert(ogl_is_succeed());
	}

	const Viewport& do_get_viewport() const override
	{
		assert(is_initialized_);
		return viewport_;
	}

	void do_set_viewport(
		const Viewport& viewport) override
	{
		if (!is_initialized_ || !is_context_current_)
		{
			assert(!"Invalid state.");
			return;
		}

		if (viewport.x_ < 0 ||
			viewport.y_ < 0 ||
			viewport.width_ <= 0 ||
			viewport.height_ <= 0 ||
			viewport.x_ >= max_viewport_size_[0] ||
			viewport.y_ >= max_viewport_size_[1] ||
			viewport.width_ >= max_viewport_size_[0] ||
			viewport.height_ >= max_viewport_size_[1])
		{
			return;
		}

		const auto is_size_changed =
			viewport.x_ != viewport_.x_ ||
			viewport.y_ != viewport_.y_ ||
			viewport.width_ != viewport_.width_ ||
			viewport.height_ != viewport_.height_;

		const auto is_depth_changed =
			viewport.depth_min_z_ != viewport_.depth_min_z_ ||
			viewport.depth_max_z_ != viewport_.depth_max_z_;

		if (!is_size_changed && !is_depth_changed)
		{
			return;
		}

		viewport_ = viewport;

		if (is_size_changed)
		{
			is_dirty_ = true;
			is_u_projection_dirty_ = true;

			set_viewport_size_internal();
		}

		if (is_depth_changed)
		{
			set_viewport_depth_internal();
		}
	}

	CullMode do_get_cull_mode() const override
	{
		assert(is_initialized_);
		return cull_mode_;
	}

	void do_set_cull_mode(
		const CullMode cull_mode) override
	{
		if (!is_initialized_ || !is_context_current_)
		{
			assert(!"Invalid state.");
			return;
		}

		if (cull_mode == cull_mode_)
		{
			return;
		}

		const auto is_old_enabled = (cull_mode_ != CullMode::disabled);
		const auto is_new_enabled = (cull_mode != CullMode::disabled);

		cull_mode_ = cull_mode;

		set_cull_mode_internal(is_old_enabled != is_new_enabled);
	}

	bool do_get_is_clipping() const override
	{
		assert(is_initialized_);
		return is_clipping_;
	}

	void do_set_is_clipping(
		const bool is_clipping) override
	{
		if (!is_initialized_ || !is_context_current_)
		{
			assert(!"Invalid state.");
			return;
		}

		if (is_clipping == is_clipping_)
		{
			return;
		}

		is_clipping_ = is_clipping;

		set_is_clipping_internal();
	}

	void set_viewport_size_internal()
	{
		const auto ogl_viewport_y = screen_height_ - (viewport_.y_ + viewport_.height_);

		::glViewport(viewport_.x_, ogl_viewport_y, viewport_.width_, viewport_.height_);
		assert(ogl_is_succeed());
	}

	void set_viewport_depth_internal()
	{
		::glDepthRange(viewport_.depth_min_z_, viewport_.depth_max_z_);
		assert(ogl_is_succeed());
	}

	bool do_is_depth_enabled() const override
	{
		assert(is_initialized_);
		return is_depth_enabled_;
	}

	void do_set_is_depth_enabled(
		const bool is_enabled) override
	{
		if (!is_initialized_ || !is_context_current_)
		{
			assert(!"Invalid state.");
			return;
		}

		if (is_enabled == is_depth_enabled_)
		{
			return;
		}

		is_depth_enabled_ = is_enabled;

		set_is_depth_enabled_internal();

		is_dirty_ = true;
		is_u_projection_dirty_ = true;
	}

	bool do_is_depth_writable() const override
	{
		assert(is_initialized_);
		return is_depth_writable_;
	}

	void do_set_is_depth_writable(
		const bool is_writable) override
	{
		if (!is_initialized_ || !is_context_current_)
		{
			assert(!"Invalid state.");
			return;
		}

		if (is_writable == is_depth_writable_)
		{
			return;
		}

		is_depth_writable_ = is_writable;

		set_is_depth_writable_internal();
	}

	DepthFunc do_get_depth_func() const override
	{
		assert(is_initialized_);
		return depth_func_;
	}

	void do_set_depth_func(
		const DepthFunc depth_func) override
	{
		if (!is_initialized_ || !is_context_current_)
		{
			assert(!"Invalid state.");
			return;
		}

		if (depth_func == depth_func_)
		{
			return;
		}

		depth_func_ = depth_func;

		set_depth_func_internal();
	}

	const float* do_get_world_matrix(
		const int index) const override
	{
		assert(is_initialized_);

		if (index < 0 || index >= VertexArrayObject::Fvf::max_tex_coord_sets)
		{
			assert(!"Invalid state.");
			return nullptr;
		}

		return world_matrices_[index].data();
	}

	void do_set_world_matrix(
		const int index,
		const float* const world_matrix_ptr) override
	{
		if (!is_initialized_ || !is_context_current_)
		{
			assert(!"Invalid state.");
			return;
		}

		if (index < 0 ||
			index >= VertexArrayObject::Fvf::max_tex_coord_sets ||
			!world_matrix_ptr)
		{
			assert(!"Invalid state.");
			return;
		}

		if ((*reinterpret_cast<const Matrix4F*>(world_matrix_ptr)) == world_matrices_[index])
		{
			return;
		}

		world_matrices_[index] = *reinterpret_cast<const Matrix4F*>(world_matrix_ptr);

		is_dirty_ = true;
		are_u_model_views_dirty_.set(index);
	}

	const float* do_get_view_matrix() const override
	{
		assert(is_initialized_);
		return view_matrix_.data();
	}

	void do_set_view_matrix(
		const float* const view_matrix_ptr) override
	{
		if (!is_initialized_ || !is_context_current_)
		{
			assert(!"Invalid state.");
			return;
		}

		if (!view_matrix_ptr)
		{
			assert(!"Invalid state.");
			return;
		}

		if ((*reinterpret_cast<const ViewMatrix*>(view_matrix_ptr)) == view_matrix_)
		{
			return;
		}

		view_matrix_ = *reinterpret_cast<const ViewMatrix*>(view_matrix_ptr);

		is_dirty_ = true;
		are_u_model_views_dirty_.set();
	}

	const float* do_get_projection_matrix() const override
	{
		assert(is_initialized_);
		return projection_matrix_.data();
	}

	void do_set_projection_matrix(
		const float* const projection_matrix_ptr) override
	{
		if (!is_initialized_ || !is_context_current_)
		{
			assert(!"Invalid state.");
			return;
		}

		if (!projection_matrix_ptr)
		{
			assert(!"Invalid state.");
			return;
		}

		if ((*reinterpret_cast<const ProjectionMatrix*>(projection_matrix_ptr)) == projection_matrix_)
		{
			return;
		}

		projection_matrix_ = *reinterpret_cast<const ProjectionMatrix*>(projection_matrix_ptr);

		is_dirty_ = true;
		is_u_projection_dirty_ = true;
	}

	VertexArrayObjectPtr do_add_vertex_array_object() override
	{
		vertex_array_objects_.emplace_back(std::make_unique<VertexArrayObjectImpl>(*this));
		return vertex_array_objects_.back().get();
	}

	void do_remove_vertex_array_object(
		VertexArrayObjectPtr vertex_array_object) override
	{
		if (vertex_array_objects_.empty())
		{
			assert(!"Empty list.");
			return;
		}

		const auto size_before = vertex_array_objects_.size();

		vertex_array_objects_.remove_if(
			[&](const auto& item_uptr)
			{
				return item_uptr.get() == vertex_array_object;
			}
		);

		const auto size_after = vertex_array_objects_.size();

		assert(size_before > size_after);
	}

	//
	// API
	// ======================================================================


	bool initialize_internal(
		const int screen_width,
		const int screen_height)
	{
		ogl_clear_error();

		if (screen_width <= 0 || screen_height <= 0)
		{
			return false;
		}

		screen_width_ = screen_width;
		screen_height_ = screen_height;

		// Get current context (must exist).
		//
		ogl_dc_ = ::wglGetCurrentDC();
		ogl_rc_ = ::wglGetCurrentContext();

		if (!ogl_dc_ || !ogl_rc_)
		{
			return false;
		}

		// Detect extensions.
		//
		detect_extensions();

		// Get implementation defined values.
		//
		::glGetIntegerv(GL_MAX_VIEWPORT_DIMS, max_viewport_size_.data());

		if (!ogl_is_succeed())
		{
			assert(!"Failed to get max viewport dimensions.");
			return false;
		}

		if (max_viewport_size_[0] <= 0 ||
			max_viewport_size_[1] <= 0 ||
			screen_width_ > max_viewport_size_[0] ||
			screen_height_ > max_viewport_size_[1])
		{
			return false;
		}

		// Load default program.
		//
		if (!load_program())
		{
			return false;
		}

		if (!enable_program(true))
		{
			return false;
		}

		if (!locate_uniforms())
		{
			return false;
		}

		// Set defaults.
		//
		if (has_gl_arb_clip_control_)
		{
			::glClipControl(GL_LOWER_LEFT, GL_ZERO_TO_ONE);
			assert(ogl_is_succeed());
		}

		set_default_clear_color();
		set_default_viewport();
		set_default_cull_mode();
		set_default_is_clipping();
		set_default_is_depth_enabled();
		set_default_is_depth_writable();
		set_default_depth_func();
		set_default_world_matrices();
		set_default_view_matrix();
		set_default_projection_matrix();
		set_uniform_defaults();

		if (!ogl_is_succeed())
		{
			assert(!"OpenGL error.");
			return false;
		}

		is_initialized_ = true;
		is_context_current_ = true;

		return true;
	}

	void uninitialize_internal()
	{
		is_initialized_ = false;
		is_context_current_ = false;

		ogl_dc_ = nullptr;
		ogl_rc_ = nullptr;

		extensions_.clear();

		has_gl_arb_clip_control_ = false;
		has_gl_ext_clip_volume_hint_ = false;

		screen_width_ = 0;
		screen_height_ = 0;

		is_dirty_ = false;

		clear_color_r_ = 0;
		clear_color_g_ = 0;
		clear_color_b_ = 0;
		clear_color_a_ = 0;

		viewport_ = {};
		max_viewport_size_ = {};

		cull_mode_ = CullMode::none;

		is_clipping_ = false;

		is_depth_enabled_ = false;
		is_depth_writable_ = false;
		depth_func_ = DepthFunc::none;

		if (program_)
		{
			::glUseProgram(0);
			assert(ogl_is_succeed());
			::glDetachShader(program_, vertex_shader_);
			assert(ogl_is_succeed());
			::glDetachShader(program_, fragment_shader_);
			assert(ogl_is_succeed());
			::glDeleteProgram(program_);
			assert(ogl_is_succeed());

			program_ = 0;
		}

		has_diffuse_ = false;
		u_has_diffuse_ = -1;

		is_position_transformed_ = false;

		are_u_model_views_dirty_ = {};
		is_u_projection_dirty_ = false;

		world_matrices_ = {};
		view_matrix_ = {};
		projection_matrix_ = {};

		u_model_views_.fill(-1);
		u_projection_ = -1;

		if (vertex_shader_)
		{
			::glDeleteShader(vertex_shader_);
			assert(ogl_is_succeed());

			vertex_shader_ = 0;
		}

		if (fragment_shader_)
		{
			::glDeleteShader(fragment_shader_);
			assert(ogl_is_succeed());

			fragment_shader_ = 0;
		}

		current_vao_ = nullptr;

		vertex_array_objects_.clear();
	}

	void set_current_context_internal(
		const bool is_current)
	{
		const auto dc = (is_current ? ogl_dc_ : nullptr);
		const auto rc = (is_current ? ogl_rc_ : nullptr);

		const auto make_result = ::wglMakeCurrent(dc, rc);
		assert(make_result);

		is_context_current_ = is_current;
	}

	void set_default_cull_mode()
	{
		::glFrontFace(GL_CW);
		assert(ogl_is_succeed());

		cull_mode_ = default_cull_mode;
		set_cull_mode_internal(true);
	}

	void set_cull_mode_internal(
		const bool enforce_cull_face)
	{
		const auto is_cull_face_enabled = (cull_mode_ != CullMode::disabled);

		switch (cull_mode_)
		{
		case CullMode::disabled:
			break;

		case CullMode::clockwise:
			::glCullFace(GL_FRONT);
			assert(ogl_is_succeed());
			break;

		case CullMode::counterclockwise:
			::glCullFace(GL_BACK);
			assert(ogl_is_succeed());
			break;

		default:
			assert(!"Invalid culling mode.");
			return;
		}

		if (enforce_cull_face)
		{
			if (is_cull_face_enabled)
			{
				::glEnable(GL_CULL_FACE);
				assert(ogl_is_succeed());
			}
			else
			{
				::glDisable(GL_CULL_FACE);
				assert(ogl_is_succeed());
			}
		}
	}

	void set_default_is_clipping()
	{
		is_clipping_ = default_is_clipping;
		set_is_clipping_internal();
	}

	void set_is_clipping_internal()
	{
		if (!has_gl_ext_clip_volume_hint_)
		{
			return;
		}

		const auto ogl_hint_value = (is_clipping_ ? GL_DONT_CARE : GL_FASTEST);

		::glHint(GL_CLIP_VOLUME_CLIPPING_HINT_EXT, ogl_hint_value);
		assert(ogl_is_succeed());
	}

	void set_default_clear_color()
	{
		clear_color_r_ = default_clear_color_r;
		clear_color_g_ = default_clear_color_g;
		clear_color_b_ = default_clear_color_b;
		clear_color_a_ = default_clear_color_a;

		set_clear_color_internal();
	}

	void set_clear_color_internal()
	{
		::glClearColor(
			clear_color_r_ / 255.0F,
			clear_color_g_ / 255.0F,
			clear_color_b_ / 255.0F,
			clear_color_a_ / 255.0F);

		assert(ogl_is_succeed());
	}

	Viewport get_default_viewport()
	{
		auto viewport = Viewport{};
		viewport.x_ = default_viewport_x;
		viewport.y_ = default_viewport_y;
		viewport.width_ = screen_width_;
		viewport.height_ = screen_height_;
		viewport.depth_min_z_ = default_viewport_depth_min_z;
		viewport.depth_max_z_ = default_viewport_depth_max_z;

		return viewport;
	}

	void set_default_viewport()
	{
		viewport_ = get_default_viewport();

		set_viewport_size_internal();
		set_viewport_depth_internal();
	}

	void set_default_is_depth_enabled()
	{
		is_depth_enabled_ = default_is_depth_enabled;
		set_is_depth_enabled_internal();
	}

	void set_is_depth_enabled_internal()
	{
		if (is_depth_enabled_)
		{
			::glEnable(GL_DEPTH_TEST);
			assert(ogl_is_succeed());
		}
		else
		{
			::glDisable(GL_DEPTH_TEST);
			assert(ogl_is_succeed());
		}
	}

	void set_default_is_depth_writable()
	{
		is_depth_writable_ = default_is_depth_writable;
		set_is_depth_writable_internal();
	}

	void set_is_depth_writable_internal()
	{
		::glDepthMask(is_depth_writable_);
		assert(ogl_is_succeed());
	}

	void set_default_depth_func()
	{
		depth_func_ = default_depth_func;
		set_depth_func_internal();
	}

	void set_depth_func_internal()
	{
		auto ogl_depth_func = GLenum{};

		switch (depth_func_)
		{
		case DepthFunc::always:
			ogl_depth_func = GL_ALWAYS;
			break;

		case DepthFunc::equal:
			ogl_depth_func = GL_EQUAL;
			break;

		case DepthFunc::greater:
			ogl_depth_func = GL_GREATER;
			break;

		case DepthFunc::greater_or_equal:
			ogl_depth_func = GL_GEQUAL;
			break;

		case DepthFunc::less:
			ogl_depth_func = GL_LESS;
			break;

		case DepthFunc::lees_or_equal:
			ogl_depth_func = GL_LEQUAL;
			break;

		case DepthFunc::not_equal:
			ogl_depth_func = GL_NOTEQUAL;
			break;

		default:
			assert(!"Invalid depth func.");
			return;
		}

		::glDepthFunc(ogl_depth_func);
		assert(ogl_is_succeed());
	}

	void set_uniform_defaults()
	{
		has_diffuse_ = default_has_diffuse;
		set_has_diffuse_internal();
	}

	void get_extensions()
	{
		auto extension_count = GLint{};

		::glGetIntegerv(GL_NUM_EXTENSIONS, &extension_count);

		extensions_.reserve(extension_count);

		for (auto i = 0; i < extension_count; ++i)
		{
			const auto extension_name = reinterpret_cast<const char*>(::glGetStringi(GL_EXTENSIONS, i));

			assert(extension_name);

			if (extension_name)
			{
				extensions_.emplace_back(extension_name);
			}
		}
	}

	bool has_extension(
		const std::string& extension_name)
	{
		if (extensions_.empty() || extension_name.empty())
		{
			return false;
		}

		const auto extension_end_it = extensions_.cend();

		const auto extension_it = std::find_if(
			extensions_.cbegin(),
			extension_end_it,
			[&](const auto& item)
			{
				return item == extension_name;
			}
		);

		return extension_it != extension_end_it;
	}

	void detect_gl_arb_clip_control_extension()
	{
		has_gl_arb_clip_control_ = has_extension("GL_ARB_clip_control");
		::glClipControl = ogl_resolve_symbol<PFNGLCLIPCONTROLPROC>("glClipControl");

		if (!has_gl_arb_clip_control_ || !::glClipControl)
		{
			has_gl_arb_clip_control_ = false;
			::glClipControl = nullptr;
		}
	}

	void detect_gl_ext_clip_volume_hint_extension()
	{
		has_gl_ext_clip_volume_hint_ = has_extension("GL_EXT_clip_volume_hint");
	}

	void detect_extensions()
	{
		get_extensions();

		detect_gl_arb_clip_control_extension();
		detect_gl_ext_clip_volume_hint_extension();
	}

	void get_shader_build_status(
		const GLenum object_status,
		void (APIENTRYP ogl_get_parameter)(GLuint object, GLenum pname, GLint* params),
		void (APIENTRYP ogl_get_log)(GLuint object, GLsizei bufSize, GLsizei* length, GLchar* infoLog),
		const GLuint object,
		bool& is_built,
		std::string& log)
	{
		is_built = false;
		log.clear();

		auto ogl_compile_status = GLint{};
		auto ogl_log_length = GLint{};

		ogl_get_parameter(object, object_status, &ogl_compile_status);
		ogl_get_parameter(object, GL_INFO_LOG_LENGTH, &ogl_log_length);

		is_built = (ogl_compile_status == GL_TRUE);

		if (ogl_log_length > 0)
		{
			log.resize(ogl_log_length);
			ogl_get_log(object, ogl_log_length, &ogl_log_length, &log[0]);
		}
	}

	void get_shader_compile_status(
		const GLuint shader,
		bool& is_compiled,
		std::string& log)
	{
		get_shader_build_status(GL_COMPILE_STATUS, ::glGetShaderiv, ::glGetShaderInfoLog, shader, is_compiled, log);
	}

	void get_program_link_status(
		const GLuint program,
		bool& is_linked,
		std::string& log)
	{
		get_shader_build_status(GL_LINK_STATUS, ::glGetProgramiv, ::glGetProgramInfoLog, program, is_linked, log);
	}

	bool load_shader(
		const ShaderType shader_type,
		GLuint& shader)
	{
		shader = GL_NONE;

		const std::string* shader_source;
		GLenum ogl_shader_type;

		switch (shader_type)
		{
		case ShaderType::vertex:
			ogl_shader_type = GL_VERTEX_SHADER;
			shader_source = &vertex_shader_source;
			break;

		case ShaderType::fragment:
			ogl_shader_type = GL_FRAGMENT_SHADER;
			shader_source = &fragment_shader_source;
			break;

		default:
			assert(!"Unsupported shader type.");
			return false;
		}

		shader = ::glCreateShader(ogl_shader_type);
		assert(ogl_is_succeed());

		if (shader == 0)
		{
			return false;
		}

		const char* const source_lines[1] =
		{
			shader_source->c_str(),
		}; // source_lines

		const GLint source_lines_lengths[1] =
		{
			static_cast<GLint>(shader_source->length()),
		}; // source_lines

		::glShaderSource(shader, 1, source_lines, source_lines_lengths);
		assert(ogl_is_succeed());
		::glCompileShader(shader);
		assert(ogl_is_succeed());

		bool is_compiled;
		std::string log;

		get_shader_compile_status(shader, is_compiled, log);

		return is_compiled;
	}

	bool load_program()
	{
		if (!load_shader(ShaderType::vertex, vertex_shader_))
		{
			return false;
		}

		if (!load_shader(ShaderType::fragment, fragment_shader_))
		{
			return false;
		}

		program_ = ::glCreateProgram();

		if (!program_)
		{
			return false;
		}

		::glAttachShader(program_, vertex_shader_);
		assert(ogl_is_succeed());
		::glAttachShader(program_, fragment_shader_);
		assert(ogl_is_succeed());
		::glLinkProgram(program_);
		assert(ogl_is_succeed());

		bool is_linked;
		std::string log;

		get_program_link_status(program_, is_linked, log);

		return is_linked;
	}

	bool enable_program(
		const bool is_enable)
	{
		::glUseProgram(is_enable ? program_ : 0);

		return ogl_is_succeed();
	}

	bool locate_uniforms()
	{
		u_has_diffuse_ = ::glGetUniformLocation(program_, "u_has_diffuse");
		assert(ogl_is_succeed());
		assert(u_has_diffuse_ >= 0);

		for (auto i = 0; i < max_world_matrices; ++i)
		{
			const auto uniform_name = "u_model_views[" + std::to_string(i) + "]";
			u_model_views_[i] = ::glGetUniformLocation(program_, uniform_name.c_str());
			assert(ogl_is_succeed());
			assert(u_model_views_[i] >= 0);
		}

		u_projection_ = ::glGetUniformLocation(program_, "u_projection");
		assert(ogl_is_succeed());
		assert(u_projection_ >= 0);

		return ogl_is_succeed();
	}

	void set_has_diffuse(
		const bool has_diffuse)
	{
		if (!is_initialized_)
		{
			return;
		}

		if (has_diffuse == has_diffuse_)
		{
			return;
		}

		has_diffuse_ = has_diffuse;

		set_has_diffuse_internal();
	}

	void set_has_diffuse_internal()
	{
		::glUniform1i(u_has_diffuse_, has_diffuse_);
		assert(ogl_is_succeed());
	}

	void set_is_position_transformed(
		const bool is_position_transformed)
	{
		if (!is_initialized_)
		{
			return;
		}

		if (is_position_transformed == is_position_transformed_)
		{
			return;
		}

		is_position_transformed_ = is_position_transformed;

		is_dirty_ = true;
		are_u_model_views_dirty_.set();
		is_u_projection_dirty_ = true;
	}

	void set_u_model_view(
		const int index)
	{
		ModelViewMatrix matrix;

		if (is_position_transformed_)
		{
			matrix = identity_matrix;
		}
		else
		{
			matrix = multiply_matrices(world_matrices_[index], view_matrix_);
		}

		::glUniformMatrix4fv(u_model_views_[index], 1, GL_FALSE, matrix.data());
		assert(ogl_is_succeed());
	}

	void set_u_projection()
	{
		ProjectionMatrix matrix;

		if (is_position_transformed_)
		{
			const auto m11 = 2.0F / viewport_.width_;
			const auto m41 = -1.0F;

			const auto m22 = -2.0F / viewport_.height_;
			const auto m42 = 1.0F;

			const auto m33 = 0.0F;
			const auto m43 = 0.0F;

			matrix =
			{
				m11, 0.0F, 0.0F, 0.0F,
				0.0F, m22, 0.0F, 0.0F,
				0.0F, 0.0F, m33, 0.0F,
				m41, m42, m43, 1.0F,
			};
		}
		else
		{
			matrix = projection_matrix_;
		}

		::glUniformMatrix4fv(u_projection_, 1, GL_FALSE, matrix.data());
		assert(ogl_is_succeed());
	}

	void set_default_world_matrices()
	{
		world_matrices_.fill(identity_matrix);

		is_dirty_ = true;
		are_u_model_views_dirty_.set();
	}

	void set_default_view_matrix()
	{
		view_matrix_ = identity_matrix;

		is_dirty_ = true;
		are_u_model_views_dirty_.set();
	}

	void set_default_projection_matrix()
	{
		projection_matrix_ = identity_matrix;

		is_dirty_ = true;
		is_u_projection_dirty_ = true;
	}

	void apply_u_view_model_changes(
		const int index)
	{
		if (!are_u_model_views_dirty_.test(index))
		{
			return;
		}

		are_u_model_views_dirty_.reset(index);

		set_u_model_view(index);
	}

	void apply_u_view_models_changes()
	{
		for (auto i = 0; i < max_world_matrices; ++i)
		{
			apply_u_view_model_changes(i);
		}
	}

	void apply_u_projection_changes()
	{
		if (!is_u_projection_dirty_)
		{
			return;
		}

		is_u_projection_dirty_ = false;

		set_u_projection();
	}

	void apply_changes()
	{
		if (!is_dirty_)
		{
			return;
		}

		is_dirty_ = false;

		apply_u_view_models_changes();
		apply_u_projection_changes();
	}


	static Matrix4F multiply_matrices(
		const Matrix4F& lhs,
		const Matrix4F& rhs)
	{
		return {
			(rhs[0] * lhs[0]) + (rhs[4] * lhs[1]) + (rhs[8] * lhs[2]) + (rhs[12] * lhs[3]),
			(rhs[1] * lhs[0]) + (rhs[5] * lhs[1]) + (rhs[9] * lhs[2]) + (rhs[13] * lhs[3]),
			(rhs[2] * lhs[0]) + (rhs[6] * lhs[1]) + (rhs[10] * lhs[2]) + (rhs[14] * lhs[3]),
			(rhs[3] * lhs[0]) + (rhs[7] * lhs[1]) + (rhs[11] * lhs[2]) + (rhs[15] * lhs[3]),
			(rhs[0] * lhs[4]) + (rhs[4] * lhs[5]) + (rhs[8] * lhs[6]) + (rhs[12] * lhs[7]),
			(rhs[1] * lhs[4]) + (rhs[5] * lhs[5]) + (rhs[9] * lhs[6]) + (rhs[13] * lhs[7]),
			(rhs[2] * lhs[4]) + (rhs[6] * lhs[5]) + (rhs[10] * lhs[6]) + (rhs[14] * lhs[7]),
			(rhs[3] * lhs[4]) + (rhs[7] * lhs[5]) + (rhs[11] * lhs[6]) + (rhs[15] * lhs[7]),
			(rhs[0] * lhs[8]) + (rhs[4] * lhs[9]) + (rhs[8] * lhs[10]) + (rhs[12] * lhs[11]),
			(rhs[1] * lhs[8]) + (rhs[5] * lhs[9]) + (rhs[9] * lhs[10]) + (rhs[13] * lhs[11]),
			(rhs[2] * lhs[8]) + (rhs[6] * lhs[9]) + (rhs[10] * lhs[10]) + (rhs[14] * lhs[11]),
			(rhs[3] * lhs[8]) + (rhs[7] * lhs[9]) + (rhs[11] * lhs[10]) + (rhs[15] * lhs[11]),
			(rhs[0] * lhs[12]) + (rhs[4] * lhs[13]) + (rhs[8] * lhs[14]) + (rhs[12] * lhs[15]),
			(rhs[1] * lhs[12]) + (rhs[5] * lhs[13]) + (rhs[9] * lhs[14]) + (rhs[13] * lhs[15]),
			(rhs[2] * lhs[12]) + (rhs[6] * lhs[13]) + (rhs[10] * lhs[14]) + (rhs[14] * lhs[15]),
			(rhs[3] * lhs[12]) + (rhs[7] * lhs[13]) + (rhs[11] * lhs[14]) + (rhs[15] * lhs[15]),
		};
	}
}; // OglRendererImpl


int OglRendererImpl::default_viewport_x = 0;
int OglRendererImpl::default_viewport_y = 0;
float OglRendererImpl::default_viewport_depth_min_z = 0.0F;
float OglRendererImpl::default_viewport_depth_max_z = 1.0F;

std::uint8_t OglRendererImpl::default_clear_color_r = 0;
std::uint8_t OglRendererImpl::default_clear_color_g = 0;
std::uint8_t OglRendererImpl::default_clear_color_b = 0;
std::uint8_t OglRendererImpl::default_clear_color_a = 0;

const OglRenderer::CullMode OglRendererImpl::default_cull_mode = OglRenderer::CullMode::counterclockwise;

const bool OglRendererImpl::default_is_clipping = true;

const bool OglRendererImpl::default_is_depth_enabled = true;
const bool OglRendererImpl::default_is_depth_writable = true;
const OglRendererImpl::DepthFunc OglRendererImpl::default_depth_func = OglRendererImpl::DepthFunc::lees_or_equal;

const bool OglRendererImpl::default_has_diffuse = false;

const OglRendererImpl::Matrix4F OglRendererImpl::identity_matrix =
{
	1.0F, 0.0F, 0.0F, 0.0F,
	0.0F, 1.0F, 0.0F, 0.0F,
	0.0F, 0.0F, 1.0F, 0.0F,
	0.0F, 0.0F, 0.0F, 1.0F,
};

const std::string OglRendererImpl::vertex_shader_source = std::string{
R"LTJS_VERTEX(

#version 330 core

// Maximum model-view matrix count.
const int max_model_view_count = 4;

// Maximum texture coordinate sets.
const int max_tcs = 4;

// Default diffuse color.
const vec4 default_diffuse = vec4(1, 1, 1, 1);


layout(location = 0) in vec4 a_position;
layout(location = 1) in vec4 a_bweights;
layout(location = 2) in vec4 a_normal;
layout(location = 3) in vec4 a_diffuse;
layout(location = 4) in vec4 a_tcs[max_tcs];


out vec4 v_diffuse;


// Has diffuse attribute?
uniform bool u_has_diffuse = false;


// Model-view matrices.
uniform mat4 u_model_views[max_model_view_count];

// Projection matrix.
uniform mat4 u_projection;


void main()
{
	if (u_has_diffuse)
	{
		v_diffuse = a_diffuse;
	}
	else
	{
		v_diffuse = default_diffuse;
	}

	gl_Position = u_projection * u_model_views[0] * a_position;
}

)LTJS_VERTEX"
}; // vertex_shader_source

const std::string OglRendererImpl::fragment_shader_source = std::string{
R"LTJS_FRAGMENT(

#version 330 core

in vec4 v_diffuse;

out vec4 o_fragment;

void main()
{
	o_fragment = v_diffuse;
}

)LTJS_FRAGMENT"
}; // fragment_shader_source

//
// OglRendererImpl
// ==========================================================================


// ==========================================================================
// OglRendererImpl::VertexArrayObjectImpl
//

OglRendererImpl::VertexArrayObjectImpl::VertexArrayObjectImpl(
	OglRendererImpl& impl)
	:
	impl_{impl},
	is_initialized_{},
	vertex_format_{},
	vertex_usage_flags_{},
	index_usage_flags_{},
	vertex_count_{},
	index_count_{},
	ogl_vao_{},
	ogl_vertex_buffer_{},
	ogl_index_buffer_{}
{
}

OglRendererImpl::VertexArrayObjectImpl::~VertexArrayObjectImpl()
{
	uninitialize_internal();
}

bool OglRendererImpl::VertexArrayObjectImpl::do_initialize(
	const InitializeParam& param)
{
	uninitialize_internal();

	if (!param.is_valid())
	{
		return false;
	}

	if (!initialize_internal(param))
	{
		uninitialize_internal();
		return false;
	}

	return true;
}

bool OglRendererImpl::VertexArrayObjectImpl::initialize_internal(
	const InitializeParam& param)
{
	::glGenVertexArrays(1, &ogl_vao_);
	assert(ogl_is_succeed());
	::glGenBuffers(1, &ogl_vertex_buffer_);
	assert(ogl_is_succeed());

	if (param.has_index_)
	{
		::glGenBuffers(1, &ogl_index_buffer_);
		assert(ogl_is_succeed());
	}


	// Vertex buffer.
	//
	const auto ogl_vertex_usage = usage_flags_to_ogl_usage(param.vertex_usage_flags_);
	const auto vertex_size = param.vertex_format_.vertex_size_;
	const auto vertex_buffer_size = vertex_size * param.vertex_count_;

	::glBindBuffer(GL_ARRAY_BUFFER, ogl_vertex_buffer_);
	assert(ogl_is_succeed());
	::glBufferData(GL_ARRAY_BUFFER, vertex_buffer_size, param.raw_vertex_data_, ogl_vertex_usage);
	assert(ogl_is_succeed());
	::glBindBuffer(GL_ARRAY_BUFFER, 0);
	assert(ogl_is_succeed());


	// Index buffer.
	//
	if (param.has_index_)
	{
		const auto ogl_index_usage = usage_flags_to_ogl_usage(param.index_usage_flags_);
		const auto index_buffer_size = index_element_size * param.index_count_;

		::glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ogl_index_buffer_);
		assert(ogl_is_succeed());
		::glBufferData(GL_ELEMENT_ARRAY_BUFFER, index_buffer_size, param.raw_index_data_, ogl_index_usage);
		assert(ogl_is_succeed());
		::glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
		assert(ogl_is_succeed());
	}


	// Record the state.
	//
	vertex_format_ = param.vertex_format_;

	const auto float_size = static_cast<int>(sizeof(float));

	auto component_offset_ptr = static_cast<const char*>(nullptr);

	if (impl_.current_vao_ != this)
	{
		impl_.current_vao_ = nullptr;
	}

	::glBindVertexArray(ogl_vao_);
	assert(ogl_is_succeed());
	::glBindBuffer(GL_ARRAY_BUFFER, ogl_vertex_buffer_);
	assert(ogl_is_succeed());

	if (param.has_index_)
	{
		::glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ogl_index_buffer_);
		assert(ogl_is_succeed());
	}

	// Position
	//
	{
		const auto position_item_count = 3 + (vertex_format_.is_position_transformed_);
		const auto position_size = position_item_count * float_size;

		::glEnableVertexAttribArray(0);
		assert(ogl_is_succeed());
		::glVertexAttribPointer(0, position_item_count, GL_FLOAT, GL_FALSE, vertex_size, component_offset_ptr);
		assert(ogl_is_succeed());

		component_offset_ptr += position_size;
	}


	// Blending weights.
	//
	if (vertex_format_.has_blending_weights())
	{
		const auto blending_weights_item_count = vertex_format_.blending_weight_count_;
		const auto blending_weights_size = blending_weights_item_count * float_size;

		::glEnableVertexAttribArray(1);
		assert(ogl_is_succeed());
		::glVertexAttribPointer(1, blending_weights_item_count, GL_FLOAT, GL_FALSE, vertex_size, component_offset_ptr);
		assert(ogl_is_succeed());

		component_offset_ptr += blending_weights_size;
	}

	// Normal.
	//
	if (vertex_format_.has_normal_)
	{
		const auto normal_item_count = 3;
		const auto normals_size = normal_item_count * float_size;

		::glEnableVertexAttribArray(2);
		assert(ogl_is_succeed());
		::glVertexAttribPointer(2, normal_item_count, GL_FLOAT, GL_FALSE, vertex_size, component_offset_ptr);
		assert(ogl_is_succeed());

		component_offset_ptr += normals_size;
	}

	if (vertex_format_.has_diffuse_)
	{
		const auto diffuse_size = float_size;

		::glEnableVertexAttribArray(3);
		assert(ogl_is_succeed());
		::glVertexAttribPointer(3, GL_BGRA, GL_UNSIGNED_BYTE, GL_TRUE, vertex_size, component_offset_ptr);
		assert(ogl_is_succeed());

		component_offset_ptr += diffuse_size;
	}

	if (vertex_format_.has_tex_coord_sets())
	{
		auto base_array_index = 4;

		for (auto i = 0; i < vertex_format_.tex_coord_set_count_; ++i)
		{
			const auto array_index = base_array_index + i;
			const auto tex_coord_set_item_count = vertex_format_.tex_coord_item_counts_[i];
			const auto tex_coord_set_size = tex_coord_set_item_count * float_size;

			::glEnableVertexAttribArray(array_index);
			assert(ogl_is_succeed());
			::glVertexAttribPointer(array_index, tex_coord_set_item_count, GL_FLOAT, GL_FALSE, vertex_size, component_offset_ptr);
			assert(ogl_is_succeed());

			component_offset_ptr += tex_coord_set_size;
		}
	}

	::glBindVertexArray(0);
	assert(ogl_is_succeed());
	::glBindBuffer(GL_ARRAY_BUFFER, 0);
	assert(ogl_is_succeed());
	::glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	assert(ogl_is_succeed());

	if (!ogl_is_succeed())
	{
		return false;
	}


	// Finalize.
	//
	is_initialized_ = true;

	vertex_usage_flags_ = param.vertex_usage_flags_;
	index_usage_flags_ = param.index_usage_flags_;
	vertex_count_ = param.vertex_count_;
	index_count_ = param.index_count_;

	return true;
}

void OglRendererImpl::VertexArrayObjectImpl::do_uninitialize()
{
	uninitialize_internal();
}

void OglRendererImpl::VertexArrayObjectImpl::do_draw(
	const int index_base,
	const int vertex_base,
	const int triangle_count)
{
	if (!is_initialized_ || index_base < 0 || vertex_base < 0 || triangle_count <= 0)
	{
		return;
	}

	impl_.set_has_diffuse(vertex_format_.has_diffuse_);
	impl_.set_is_position_transformed(vertex_format_.is_position_transformed_);
	impl_.apply_changes();

	if (impl_.current_vao_ != this)
	{
		impl_.current_vao_ = this;

		::glBindVertexArray(ogl_vao_);
		assert(ogl_is_succeed());
	}

	if (index_count_ > 0)
	{
		::glDrawElementsBaseVertex(
			GL_TRIANGLES,
			triangle_count * 3,
			GL_UNSIGNED_SHORT,
			reinterpret_cast<const char*>(static_cast<std::intptr_t>(index_base)),
			vertex_base);

		assert(ogl_is_succeed());
	}
	else
	{
		::glDrawArrays(GL_TRIANGLES, vertex_base * 3, triangle_count * 3);
		assert(ogl_is_succeed());
	}
}

void OglRendererImpl::VertexArrayObjectImpl::uninitialize_internal()
{
	is_initialized_ = false;
	vertex_format_ = {};
	vertex_usage_flags_ = UsageFlags::none;
	index_usage_flags_ = UsageFlags::none;
	vertex_count_ = 0;
	index_count_ = 0;

	if (ogl_vao_)
	{
		if (impl_.current_vao_ == this)
		{
			impl_.current_vao_ = nullptr;

			::glBindVertexArray(0);
			assert(ogl_is_succeed());
		}

		::glDeleteVertexArrays(1, &ogl_vao_);
		assert(ogl_is_succeed());

		ogl_vao_ = 0;
	}

	if (ogl_vertex_buffer_)
	{
		::glDeleteBuffers(1, &ogl_vertex_buffer_);
		assert(ogl_is_succeed());

		ogl_vertex_buffer_ = 0;
	}

	if (ogl_index_buffer_)
	{
		::glDeleteBuffers(1, &ogl_index_buffer_);
		assert(ogl_is_succeed());

		ogl_index_buffer_ = 0;
	}
}

//
// OglRendererImpl::VertexArrayObjectImpl
// ==========================================================================


// ==========================================================================
// OglRendererImpl::VertexArrayObject
//

bool OglRendererImpl::VertexArrayObject::initialize(
	const InitializeParam& param)
{
	return do_initialize(param);
}

void OglRendererImpl::VertexArrayObject::uninitialize()
{
	return do_uninitialize();
}

void OglRendererImpl::VertexArrayObject::draw(
	const int triangle_count)
{
	do_draw(0, 0, triangle_count);
}

void OglRendererImpl::VertexArrayObject::draw(
	const int vertex_base,
	const int triangle_count)
{
	do_draw(0, vertex_base, triangle_count);
}

void OglRendererImpl::VertexArrayObject::draw(
	const int index_base,
	const int vertex_base,
	const int triangle_count)
{
	do_draw(index_base, vertex_base, triangle_count);
}

//
// OglRendererImpl::VertexArrayObjectImpl
// ==========================================================================


// ==========================================================================
// OglRenderer::Viewport
//

OglRenderer::Viewport::Viewport()
	:
	x_{},
	y_{},
	width_{},
	height_{},
	depth_min_z_{},
	depth_max_z_{}
{
}

//
// OglRenderer::Viewport
// ==========================================================================


// ==========================================================================
// OglRenderer::VertexArrayObject::Fvf
//

OglRenderer::VertexArrayObject::Fvf::Fvf()
	:
	has_position_{},
	is_position_transformed_{},
	blending_weight_count_{},
	has_normal_{},
	has_diffuse_{},
	tex_coord_set_count_{},
	tex_coord_item_counts_{},
	vertex_size_{}
{
}

OglRenderer::VertexArrayObject::Fvf::Fvf(
	const std::uint32_t d3d_fvf)
{
	*this = from_d3d(d3d_fvf);
}

bool OglRenderer::VertexArrayObject::Fvf::has_blending_weights() const
{
	return blending_weight_count_ > 0;
}

bool OglRenderer::VertexArrayObject::Fvf::has_tex_coord_sets() const
{
	return tex_coord_set_count_ > 0;
}

bool OglRenderer::VertexArrayObject::Fvf::is_valid() const
{
	return has_position_ && vertex_size_ > 0;
}

OglRenderer::VertexArrayObject::Fvf OglRenderer::VertexArrayObject::Fvf::from_d3d(
	const std::uint32_t d3d_fvf)
{
	auto is_zero = false;

	if (d3d_fvf == 0)
	{
		is_zero = true;
	}

	if ((d3d_fvf & d3dfvf_valid_flags) != d3d_fvf)
	{
		assert(!"Unsupported flags.");
		is_zero = true;
	}

	if (is_zero)
	{
		return {};
	}

	auto has_transformed = false;
	auto has_untransformed = false;

	if (((d3d_fvf & d3dfvf_xyz) == d3dfvf_xyz && (d3d_fvf & d3dfvf_xyzrhw) != d3dfvf_xyzrhw) ||
		(d3d_fvf & d3dfvf_xyzb1) == d3dfvf_xyzb1 ||
		(d3d_fvf & d3dfvf_xyzb2) == d3dfvf_xyzb2 ||
		(d3d_fvf & d3dfvf_xyzb3) == d3dfvf_xyzb3)
	{
		has_untransformed = true;
	}

	if ((d3d_fvf & d3dfvf_xyzrhw) == d3dfvf_xyzrhw)
	{
		has_transformed = true;
	}

	if (has_transformed && has_untransformed)
	{
		assert(!"Transformed and untransformed are mutual exclusive.");
		return {};
	}

	auto has_normal = false;

	if ((d3d_fvf & d3dfvf_normal) == d3dfvf_normal)
	{
		has_normal = true;
	}

	if (has_transformed && has_normal)
	{
		assert(!"Transformed and normal are prohibited.");
		return {};
	}


	auto fvf = Fvf{};

	if (has_transformed)
	{
		fvf.has_position_ = true;
		fvf.is_position_transformed_ = true;
	}

	if (has_untransformed)
	{
		fvf.has_position_ = true;
		fvf.is_position_transformed_ = false;
	}

	if ((d3d_fvf & d3dfvf_xyzb1) == d3dfvf_xyzb1)
	{
		fvf.has_position_ = true;
		fvf.blending_weight_count_ = 1;
	}

	if ((d3d_fvf & d3dfvf_xyzb2) == d3dfvf_xyzb2)
	{
		fvf.has_position_ = true;
		fvf.blending_weight_count_ = 2;
	}

	if ((d3d_fvf & d3dfvf_xyzb3) == d3dfvf_xyzb3)
	{
		fvf.has_position_ = true;
		fvf.blending_weight_count_ = 3;
	}

	if ((d3d_fvf & d3dfvf_normal) == d3dfvf_normal)
	{
		fvf.has_normal_ = true;
	}

	if ((d3d_fvf & d3dfvf_diffuse) == d3dfvf_diffuse)
	{
		fvf.has_diffuse_ = true;
	}

	if ((d3d_fvf & d3dfvf_tex1) == d3dfvf_tex1)
	{
		fvf.tex_coord_set_count_ = 1;
	}

	if ((d3d_fvf & d3dfvf_tex2) == d3dfvf_tex2)
	{
		fvf.tex_coord_set_count_ = 2;
	}

	if ((d3d_fvf & d3dfvf_tex3) == d3dfvf_tex3)
	{
		fvf.tex_coord_set_count_ = 3;
	}

	if ((d3d_fvf & d3dfvf_tex4) == d3dfvf_tex4)
	{
		fvf.tex_coord_set_count_ = 4;
	}

	for (auto i = 0; i < fvf.tex_coord_set_count_; ++i)
	{
		if ((d3d_fvf & d3dfvf_texcoordsize4(i)) == d3dfvf_texcoordsize4(i))
		{
			fvf.tex_coord_item_counts_[i] = 4;
		}
		else if ((d3d_fvf & d3dfvf_texcoordsize3(i)) == d3dfvf_texcoordsize3(i))
		{
			fvf.tex_coord_item_counts_[i] = 3;
		}
		else
		{
			fvf.tex_coord_item_counts_[i] = 2;
		}
	}


	// Calculate a size of the vertex.
	// Each component item has four byte size (float, DWORD).
	//
	fvf.vertex_size_ = 4 * (
		(fvf.has_position_ ? (fvf.is_position_transformed_ ? 4 : 3) : 0) +
		fvf.blending_weight_count_ +
		(fvf.has_normal_ ? 3 : 0) +
		(fvf.has_diffuse_ ? 1 : 0) +
		std::accumulate(
			fvf.tex_coord_item_counts_.cbegin(),
			fvf.tex_coord_item_counts_.cbegin() + fvf.tex_coord_set_count_,
			0) +
		0);

	return fvf;
}

//
// OglRenderer::VertexArrayObject::Fvf
// ==========================================================================


// ==========================================================================
// OglRenderer::VertexArrayObject::InitializeParam
//

OglRenderer::VertexArrayObject::InitializeParam::InitializeParam()
	:
	has_index_{},
	vertex_format_{},
	vertex_usage_flags_{},
	vertex_count_{},
	raw_vertex_data_{},
	index_usage_flags_{},
	index_count_{},
	raw_index_data_{}
{
}

bool OglRenderer::VertexArrayObject::InitializeParam::is_valid() const
{
	if (!vertex_format_.is_valid())
	{
		return false;
	}

	const auto ogl_vertex_usage = usage_flags_to_ogl_usage(vertex_usage_flags_);

	if (!ogl_vertex_usage || vertex_count_ <= 0 || !raw_vertex_data_)
	{
		return false;
	}

	if (has_index_)
	{
		const auto ogl_index_usage = usage_flags_to_ogl_usage(index_usage_flags_);

		if (!ogl_index_usage || index_count_ <= 0 || !raw_index_data_)
		{
			return false;
		}
	}

	return true;
}

//
// OglRenderer::VertexArrayObject::InitializeParam
// ==========================================================================


// ==========================================================================
// OglRenderer::VertexArrayObject
//

OglRenderer::VertexArrayObject::VertexArrayObject()
{
}

OglRenderer::VertexArrayObject::~VertexArrayObject()
{
}

//
// OglRenderer::VertexArrayObject
// ==========================================================================


// ==========================================================================
// OglRenderer
//

OglRenderer::OglRenderer()
{
}

OglRenderer::~OglRenderer()
{
}

bool OglRenderer::is_initialized() const
{
	return do_is_initialized();
}

bool OglRenderer::initialize(
	const int screen_width,
	const int screen_height)
{
	return do_initialize(screen_width, screen_height);
}

void OglRenderer::uninitialize()
{
	do_set_current_context(true);
	do_uninitialize();
}

void OglRenderer::set_current_context(
	const bool is_current)
{
	do_set_current_context(is_current);
}

void OglRenderer::set_clear_color(
	const std::uint8_t r,
	const std::uint8_t g,
	const std::uint8_t b,
	const std::uint8_t a)
{
	do_set_clear_color(r, g, b, a);
}

const OglRenderer::Viewport& OglRenderer::get_viewport() const
{
	return do_get_viewport();
}

void OglRenderer::clear(
	const ClearFlags clear_flags)
{
	do_clear(clear_flags);
}

void OglRenderer::set_viewport(
	const Viewport& viewport)
{
	do_set_viewport(viewport);
}

OglRenderer::CullMode OglRenderer::get_cull_mode() const
{
	return do_get_cull_mode();
}

void OglRenderer::set_cull_mode(
	const CullMode cull_mode)
{
	do_set_cull_mode(cull_mode);
}

bool OglRenderer::get_is_clipping() const
{
	return do_get_is_clipping();
}

void OglRenderer::set_is_clipping(
	const bool is_clipping)
{
	do_set_is_clipping(is_clipping);
}

bool OglRenderer::is_depth_enabled() const
{
	return do_is_depth_enabled();
}

void OglRenderer::set_is_depth_enabled(
	const bool is_enabled)
{
	do_set_is_depth_enabled(is_enabled);
}

bool OglRenderer::is_depth_writable() const
{
	return do_is_depth_writable();
}

void OglRenderer::set_is_depth_writable(
	const bool is_writable)
{
	do_set_is_depth_writable(is_writable);
}

OglRenderer::DepthFunc OglRenderer::get_depth_func() const
{
	return do_get_depth_func();
}

void OglRenderer::set_depth_func(
	const DepthFunc depth_func)
{
	do_set_depth_func(depth_func);
}

const float* OglRenderer::get_world_matrix(
	const int index) const
{
	return do_get_world_matrix(index);
}

void OglRenderer::set_world_matrix(
	const int index,
	const float* const world_matrix_ptr)
{
	do_set_world_matrix(index, world_matrix_ptr);
}

const float* OglRenderer::get_view_matrix() const
{
	return do_get_view_matrix();
}

void OglRenderer::set_view_matrix(
	const float* const view_matrix_ptr)
{
	do_set_view_matrix(view_matrix_ptr);
}

const float* OglRenderer::get_projection_matrix() const
{
	return do_get_projection_matrix();
}

void OglRenderer::set_projection_matrix(
	const float* const projection_matrix_ptr)
{
	do_set_projection_matrix(projection_matrix_ptr);
}

OglRenderer::VertexArrayObjectPtr OglRenderer::add_vertex_array_object()
{
	return do_add_vertex_array_object();
}

void OglRenderer::remove_vertex_array_object(
	VertexArrayObjectPtr vertex_array_object)
{
	do_remove_vertex_array_object(vertex_array_object);
}

void OglRenderer::ogl_clear_error()
{
	static_cast<void>(ogl_is_succeed());
}

bool OglRenderer::ogl_is_succeed()
{
	const auto max_error_flags = 5;

	auto is_failed = false;

	for (auto i = 0; i < (max_error_flags + 1); ++i)
	{
		const auto ogl_error = ::glGetError();

		if (ogl_error == GL_NO_ERROR)
		{
			break;
		}
		else
		{
			is_failed = true;
		}

		if (i == max_error_flags)
		{
			assert(!"No current context.");
			break;
		}
	}

	return !is_failed;
}

OglRenderer& OglRenderer::get_instance()
{
	static OglRendererImpl instance{};
	return instance;
}

//
// OglRenderer
// ==========================================================================


} // ltjs

