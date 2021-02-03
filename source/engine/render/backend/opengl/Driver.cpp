#include <common/Log.h>
#include <render/backend/opengl/Driver.h>
#include <render/backend/opengl/Utils.h>

#include <glad_loader.h>

#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <cstring>

namespace render::backend::opengl
{

static void GLAPIENTRY debugLog(
	GLenum source,
	GLenum type,
	GLuint id,
	GLenum severity,
	GLsizei length,
	const GLchar *message,
	const void *user_data
)
{
	Log::error("---------------\n");
	Log::error("opengl::Driver: (%d) %s\n", id, message);

	switch (source)
	{
		case GL_DEBUG_SOURCE_API:				Log::error("Source: API\n"); break;
		case GL_DEBUG_SOURCE_WINDOW_SYSTEM:		Log::error("Source: Window System\n"); break;
		case GL_DEBUG_SOURCE_SHADER_COMPILER:	Log::error("Source: Shader Compiler\n"); break;
		case GL_DEBUG_SOURCE_THIRD_PARTY:		Log::error("Source: Third Party\n"); break;
		case GL_DEBUG_SOURCE_APPLICATION:		Log::error("Source: Application\n"); break;
		case GL_DEBUG_SOURCE_OTHER:				Log::error("Source: Other\n"); break;
	}

	switch (type)
	{
		case GL_DEBUG_TYPE_ERROR:				Log::error("Type: Error\n"); break;
		case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR:	Log::error("Type: Deprecated Behaviour\n"); break;
		case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:	Log::error("Type: Undefined Behaviour\n"); break;
		case GL_DEBUG_TYPE_PORTABILITY:			Log::error("Type: Portability\n"); break;
		case GL_DEBUG_TYPE_PERFORMANCE:			Log::error("Type: Performance\n"); break;
		case GL_DEBUG_TYPE_MARKER:				Log::error("Type: Marker\n"); break;
		case GL_DEBUG_TYPE_PUSH_GROUP:			Log::error("Type: Push Group\n"); break;
		case GL_DEBUG_TYPE_POP_GROUP:			Log::error("Type: Pop Group\n"); break;
		case GL_DEBUG_TYPE_OTHER:				Log::error("Type: Other\n"); break;
	}

	switch (severity)
	{
		case GL_DEBUG_SEVERITY_HIGH:			Log::error("Severity: high\n"); break;
		case GL_DEBUG_SEVERITY_MEDIUM:			Log::error("Severity: medium\n"); break;
		case GL_DEBUG_SEVERITY_LOW:				Log::error("Severity: low\n"); break;
		case GL_DEBUG_SEVERITY_NOTIFICATION:	Log::error("Severity: notification\n"); break;
	}
 	Log::error("\n");
}

//
Driver::Driver(const char *application_name, const char *engine_name)
{
	// TODO: implement
}

Driver::~Driver()
{
	// TODO: implement
}

//
bool Driver::init()
{
	if (!gladLoaderInit())
	{
		Log::fatal("opengl::Driver::init(): failed to initialize GLAD loader\n");
		return false;
	}

	if (!Platform::init())
	{
		Log::fatal("opengl::Driver::init(): failed to initialize platform\n");
		return false;
	}

	if (!gladLoadGLLoader(&gladLoaderProc))
	{
		Log::fatal("opengl::Driver::init(): failed to load OpenGL\n");
		return false;
	}

	if (!GLAD_GL_VERSION_4_5)
	{
		Log::fatal("opengl::Driver::init(): this device does not support OpenGL 4.5\n");
		return false;
	}

	glEnable(GL_MULTISAMPLE);

	memset(&pipeline_state, 0, sizeof(PipelineState));
	memset(&pipeline_state_overrides, 0, sizeof(PipelineStateOverrides));

	return true;
}

void Driver::shutdown()
{
	glDeleteProgramPipelines(1, &graphics_pipeline_id);
	graphics_pipeline_id = GL_NONE;

	Platform::shutdown();
}

void Driver::fetchGraphicsPipeline()
{
	if (graphics_pipeline_id != GL_NONE)
		return;

	glGenProgramPipelines(1, &graphics_pipeline_id);
	glBindProgramPipeline(graphics_pipeline_id);

	assert(glIsProgramPipeline(graphics_pipeline_id) == GL_TRUE);
}

void Driver::flushPipelineState()
{
	if (!pipeline_dirty)
		return;

	pipeline_dirty = false;

	// viewport
	if (pipeline_state_overrides.viewport)
		glViewport(pipeline_state.viewport_x, pipeline_state.viewport_y, pipeline_state.viewport_width, pipeline_state.viewport_height);

	// depth state
	if (pipeline_state_overrides.depth_test)
	{
		if (pipeline_state.depth_test)
			glEnable(GL_DEPTH_TEST);
		else
			glDisable(GL_DEPTH_TEST);
	}

	if (pipeline_state_overrides.depth_write)
		glDepthMask(pipeline_state.depth_write);

	if (pipeline_state_overrides.depth_comparison_func)
		glDepthFunc(pipeline_state.depth_comparison_func);

	// blending
	if (pipeline_state_overrides.blend)
	{
		if (pipeline_state.blend)
			glEnable(GL_BLEND);
		else
			glDisable(GL_BLEND);
	}

	if (pipeline_state_overrides.blend_factors)
		glBlendFunc(pipeline_state.blend_src_factor, pipeline_state.blend_dst_factor);

	// cull mode
	if (pipeline_state_overrides.cull_mode)
	{
		if (pipeline_state.cull_mode == GL_NONE)
			glDisable(GL_CULL_FACE);
		else
		{
			glEnable(GL_CULL_FACE);
			glCullFace(pipeline_state.cull_mode);
		}
	}

	// textures
	for (uint16_t i = 0; i < MAX_TEXTURES; ++i)
	{
		if ((pipeline_state_overrides.bound_textures & (1 << i)) == 0)
			continue;

		glActiveTexture(GL_TEXTURE0 + i);
		glBindTexture(pipeline_state.bound_texture_types[i], pipeline_state.bound_textures[i]);
	}

	// ubos
	for (uint16_t i = 0; i < MAX_UNIFORM_BUFFERS; ++i)
	{
		if ((pipeline_state_overrides.bound_uniform_buffers & (1 << i)) == 0)
			continue;

		glBindBufferBase(GL_UNIFORM_BUFFER, i, pipeline_state.bound_uniform_buffers[i]);
	}

	// shaders
	fetchGraphicsPipeline();

	for (uint16_t i = 0; i < MAX_SHADERS; ++i)
	{
		if ((pipeline_state_overrides.bound_shaders & (1 << i)) == 0)
			continue;

		GLenum gl_shader_stage_bitmask = Utils::getShaderStageBitmask(static_cast<ShaderType>(i));

		glUseProgramStages(graphics_pipeline_id, gl_shader_stage_bitmask, pipeline_state.bound_shaders[i]);
	}

	pipeline_state_overrides = {};
}

//
backend::VertexBuffer *Driver::createVertexBuffer(
	BufferType type,
	uint16_t vertex_size,
	uint32_t num_vertices,
	uint8_t num_attributes,
	const backend::VertexAttribute *attributes,
	const void *data
)
{
	VertexBuffer *result = new VertexBuffer();

	result->data = Utils::createImmutableBuffer(GL_ARRAY_BUFFER, vertex_size * num_vertices, data);
	result->num_vertices = num_vertices;
	result->vertex_size = vertex_size;

	glGenVertexArrays(1, &result->vao_id);
	glBindVertexArray(result->vao_id);

	glBindBuffer(result->data->type, result->data->id);
	for(int i = 0; i < num_attributes; i++)
	{
		const VertexAttribute &attribute = attributes[i];

		GLint num_elements = Utils::getAttributeComponents(attribute.format);
		GLenum type = Utils::getAttributeType(attribute.format);
		GLboolean normalized = Utils::isAttributeNormalized(attribute.format);

		glVertexAttribPointer(i, num_elements, type, normalized, vertex_size, (const char *)0 + attribute.offset);
		glEnableVertexAttribArray(i);
	}

	glBindBuffer(result->data->type, result->data->id);
	glBindVertexArray(0);

	return result;
}

backend::IndexBuffer *Driver::createIndexBuffer(
	BufferType type,
	IndexFormat index_format,
	uint32_t num_indices,
	const void *data
)
{
	IndexBuffer *result = new IndexBuffer();

	result->data = Utils::createImmutableBuffer(GL_ELEMENT_ARRAY_BUFFER, Utils::getIndexSize(index_format) * num_indices, data);
	result->num_indices = num_indices;
	result->index_format = Utils::getIndexFormat(index_format);

	return result;
}

//
backend::Texture *Driver::createTexture2D(
	uint32_t width,
	uint32_t height,
	uint32_t num_mipmaps,
	Format format,
	const void *data,
	uint32_t num_data_mipmaps
)
{
	Texture *result = new Texture();

	result->type = GL_TEXTURE_2D;
	result->internal_format = Utils::getInternalFormat(format);
	result->width = width;
	result->height = height;
	result->mips = static_cast<GLint>(num_mipmaps);

	glGenTextures(1, &result->id);
	glBindTexture(result->type, result->id);
	glTexStorage2D(result->type, result->mips, result->internal_format, result->width, result->height);

	if (data != nullptr)
	{
		GLint pixel_size = Utils::getPixelSize(format);
		GLenum pixel_type = Utils::getPixelType(format);
		GLenum gl_format = Utils::getFormat(format);
		const unsigned char *mip_data = reinterpret_cast<const unsigned char *>(data);

		GLint mip_width = result->width;
		GLint mip_height = result->height;

		for (uint32_t mip = 0; mip < num_data_mipmaps; mip++)
		{
			glTexSubImage2D(result->type, mip, 0, 0, mip_width, mip_height, gl_format, pixel_type, mip_data);

			mip_data += mip_width * mip_height * pixel_size;
			mip_width = std::max<GLint>(1, mip_width / 2);
			mip_height = std::max<GLint>(1, mip_height / 2);
		}
	}

	Utils::setDefaulTextureParameters(result);
	return result;
}

backend::Texture *Driver::createTexture2DMultisample(
	uint32_t width,
	uint32_t height,
	Format format,
	Multisample samples
)
{
	Texture *result = new Texture();

	result->type = GL_TEXTURE_2D_MULTISAMPLE;
	result->internal_format = Utils::getInternalFormat(format);
	result->width = width;
	result->height = height;
	result->num_samples = Utils::getSampleCount(samples);

	glGenTextures(1, &result->id);
	glBindTexture(result->type, result->id);

	glTexImage2DMultisample(result->type, result->num_samples, result->internal_format, result->width, result->height, GL_FALSE);

	Utils::setDefaulTextureParameters(result);
	return result;
}

backend::Texture *Driver::createTexture2DArray(
	uint32_t width,
	uint32_t height,
	uint32_t num_mipmaps,
	uint32_t num_layers,
	Format format,
	const void *data,
	uint32_t num_data_mipmaps,
	uint32_t num_data_layers
)
{
	assert(num_layers > 0);

	Texture *result = new Texture();

	result->type = GL_TEXTURE_2D_ARRAY;
	result->internal_format = Utils::getInternalFormat(format);
	result->width = width;
	result->height = height;
	result->layers = num_layers;
	result->mips = num_mipmaps;

	glGenTextures(1, &result->id);
	glBindTexture(result->type, result->id);
	glTexStorage3D(result->type, result->mips, result->internal_format, result->width, result->height, result->layers);

	if (data != nullptr)
	{
		GLint pixel_size = Utils::getPixelSize(format);
		GLenum pixel_type = Utils::getPixelType(format);
		GLenum gl_format = Utils::getFormat(format);
		const unsigned char *mip_data = reinterpret_cast<const unsigned char *>(data);

		for (uint32_t layer = 0; layer < num_data_layers; layer++)
		{
			GLint mip_width = result->width;
			GLint mip_height = result->height;
			for (uint32_t mip = 0; mip < num_data_mipmaps; mip++)
			{
				glTexSubImage3D(result->type, mip, 0, 0, layer, mip_width, mip_height, 1, gl_format, pixel_type, mip_data);

				mip_data += mip_width * mip_height * pixel_size;
				mip_width = std::max<GLint>(1, mip_width / 2);
				mip_height = std::max<GLint>(1, mip_height / 2);
			}
		}
	}

	Utils::setDefaulTextureParameters(result);
	return result;
}

backend::Texture *Driver::createTexture3D(
	uint32_t width,
	uint32_t height,
	uint32_t depth,
	uint32_t num_mipmaps,
	Format format,
	const void *data,
	uint32_t num_data_mipmaps
)
{
	Texture *result = new Texture();

	result->type = GL_TEXTURE_3D;
	result->internal_format = Utils::getInternalFormat(format);
	result->width = width;
	result->height = height;
	result->depth = depth;
	result->mips = num_mipmaps;

	glGenTextures(1, &result->id);
	glBindTexture(result->type, result->id);
	glTexStorage3D(result->type, result->mips, result->internal_format, result->width, result->height, result->depth);

	if (data != nullptr)
	{
		GLint pixel_size = Utils::getPixelSize(format);
		GLenum pixel_type = Utils::getPixelType(format);
		GLenum gl_format = Utils::getFormat(format);
		const unsigned char *mip_data = reinterpret_cast<const unsigned char *>(data);

		GLint mip_width = result->width;
		GLint mip_height = result->height;
		GLint mip_depth = result->depth;

		for (uint32_t mip = 0; mip < num_data_mipmaps; mip++)
		{
			glTexSubImage3D(result->type, mip, 0, 0, 0, mip_width, mip_height, mip_depth, gl_format, pixel_type, mip_data);

			mip_data += mip_width * mip_height * mip_depth * pixel_size;
			mip_width = std::max<GLint>(1, mip_width / 2);
			mip_height = std::max<GLint>(1, mip_height / 2);
			mip_depth =  std::max<GLint>(1, mip_depth / 2);
		}
	}

	Utils::setDefaulTextureParameters(result);
	return result;
}

backend::Texture *Driver::createTextureCube(
	uint32_t size,
	uint32_t num_mipmaps,
	Format format,
	const void *data,
	uint32_t num_data_mipmaps
)
{
	Texture *result = new Texture();

	result->type = GL_TEXTURE_CUBE_MAP;
	result->internal_format = Utils::getInternalFormat(format);
	result->width = size;
	result->height = size;
	result->layers = 6;
	result->mips = num_mipmaps;

	glGenTextures(1, &result->id);
	glBindTexture(result->type, result->id);

	GLenum pixel_type = Utils::getPixelType(format);
	GLenum gl_format = Utils::getFormat(format);

	GLint mip_width = result->width;
	GLint mip_height = result->height;

	for (uint32_t mip = 0; mip < num_mipmaps; mip++)
	{
		for (uint32_t layer = 0; layer < 6; layer++)
			glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + layer, mip, result->internal_format, mip_width, mip_height, 0, gl_format, pixel_type, nullptr);

		mip_width = std::max<GLint>(1, mip_width / 2);
		mip_height = std::max<GLint>(1, mip_height / 2);
	}

	if (data != nullptr)
	{
		GLint pixel_size = Utils::getPixelSize(format);
		const unsigned char *mip_data = reinterpret_cast<const unsigned char *>(data);

		for (uint32_t layer = 0; layer < 6; layer++)
		{
			mip_width = result->width;
			mip_height = result->height;

			for (uint32_t mip = 0; mip < num_data_mipmaps; mip++)
			{
				glTexSubImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + layer, mip, 0, 0, mip_width, mip_height, gl_format, pixel_type, mip_data);

				mip_data += mip_width * mip_height * pixel_size;
				mip_width = std::max<GLint>(1, mip_width / 2);
				mip_height = std::max<GLint>(1, mip_height / 2);
			}
		}
	}

	glTexParameteri(result->type, GL_TEXTURE_BASE_LEVEL, 0);
	glTexParameteri(result->type, GL_TEXTURE_MAX_LEVEL, result->mips - 1);

	Utils::setDefaulTextureParameters(result);
	return result;
}

//
backend::FrameBuffer *Driver::createFrameBuffer(
	uint8_t num_color_attachments,
	const FrameBufferAttachment *color_attachments,
	const FrameBufferAttachment *depthstencil_attachment
)
{
	assert((depthstencil_attachment != nullptr) || (num_color_attachments > 0 && color_attachments != nullptr));

	FrameBuffer *result = new FrameBuffer();
	result->num_color_attachments = num_color_attachments;

	glGenFramebuffers(1, &result->id);
	glBindFramebuffer(GL_FRAMEBUFFER, result->id);

	GLenum draw_buffers[FrameBuffer::MAX_COLOR_ATTACHMENTS];

	for (uint8_t i = 0; i < num_color_attachments; ++i)
	{
		const FrameBufferAttachment &attachment = color_attachments[i];
		const Texture *gl_texture = static_cast<const Texture *>(attachment.texture);

		GLenum attachment_type = GL_COLOR_ATTACHMENT0 + i;
		GLint mip = attachment.base_mip;
		GLint layer = attachment.base_layer;
		GLuint num_layers = attachment.num_layers;

		switch (gl_texture->type)
		{
			case GL_TEXTURE_2D:
				glFramebufferTexture2D(GL_FRAMEBUFFER, attachment_type, gl_texture->type, gl_texture->id, mip);
			break;
			case GL_TEXTURE_CUBE_MAP:
			{
				if (num_layers == 6)
					glFramebufferTexture(GL_FRAMEBUFFER, attachment_type, gl_texture->id, mip);
				else
					glFramebufferTexture2D(GL_FRAMEBUFFER, attachment_type, GL_TEXTURE_CUBE_MAP_POSITIVE_X + layer, gl_texture->id, mip);
			}
			break;
			case GL_TEXTURE_2D_ARRAY:
			case GL_TEXTURE_3D:
				glFramebufferTextureLayer(GL_FRAMEBUFFER, attachment_type, gl_texture->id, mip, layer);
			break;
		}

		draw_buffers[i] = attachment_type;

		result->color_attachments[i].id = gl_texture->id;
		result->color_attachments[i].base_mip = mip;
		result->color_attachments[i].base_layer = layer;
		result->color_attachments[i].num_layers = num_layers;
	}

	if (depthstencil_attachment != nullptr)
	{
		const Texture *gl_texture = static_cast<const Texture *>(depthstencil_attachment->texture);
		GLenum attachment_type = Utils::getFramebufferDepthStencilAttachmentType(gl_texture->internal_format);

		glFramebufferTexture2D(GL_FRAMEBUFFER, attachment_type, GL_TEXTURE_2D, gl_texture->id, 0);

		result->depthstencil_attachment.id = gl_texture->id;
	}

	glDrawBuffers(result->num_color_attachments, draw_buffers);

	GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	if (status != GL_FRAMEBUFFER_COMPLETE)
	{
		Log::error("opengl::Driver::createFramebuffer(): framebuffer is not complete, error: %d\n", status);
		destroyFrameBuffer(result);
		return nullptr;
	}

	return result;
}

backend::CommandBuffer *Driver::createCommandBuffer(
	CommandBufferType type
)
{
	// TODO: implement
	return nullptr;
}

//
backend::UniformBuffer *Driver::createUniformBuffer(
	BufferType type,
	uint32_t size,
	const void *data
)
{
	UniformBuffer *result = new UniformBuffer();

	result->data = Utils::createImmutableBuffer(GL_UNIFORM_BUFFER, size, data, GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT);

	return result;
}

//
backend::Shader *Driver::createShaderFromSource(
	ShaderType type,
	uint32_t size,
	const char *data,
	const char *path
)
{
	return nullptr;
	// TODO: implement
	/*
	std::vector<const char *> modified_sources(num_sources + 1);
	std::vector<GLint> modified_sizes(num_sources + 1);

	static const char *header =
		"#version 450 core\n"
		"#extension GL_ARB_separate_shader_objects: require\n";

	modified_sources[0] = header;
	modified_sizes[0] = static_cast<GLint>(strlen(header));

	for (uint32_t i = 0; i < num_sources; ++i)
	{
		modified_sources[i + 1] = sources[i];
		modified_sizes[i + 1] = static_cast<GLint>(sizes[i]);
	}

	GLenum shader_type = Utils::getShaderType(type);
	GLuint shader = glCreateShader(shader_type);
	if (shader == 0)
	{
		Log::error("opengl::Driver::createShaderFromSource(): glCreateShader failed, path: \"%s\"\n", path);
		return nullptr;
	}

	glShaderSource(shader, num_sources + 1, modified_sources.data(), modified_sizes.data());
	glCompileShader(shader);

	GLint compiled = GL_FALSE;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);

	if (!compiled)
	{
		GLint length = 0;
		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);

		std::vector<GLchar> info_log(length);
		glGetShaderInfoLog(shader, length, &length, info_log.data());

		Log::error("opengl::Driver::createShaderFromSource(): compilation failed, path: \"%s\", error: %s\n", path, info_log.data());
	}

	GLuint program = glCreateProgram();
	if (program == 0)
	{
		glDeleteShader(shader);
		Log::error("opengl::Driver::createShaderFromSource(): glCreateProgram failed, path: \"%s\"\n", path);
		return nullptr;
	}
	glProgramParameteri(program, GL_PROGRAM_SEPARABLE, GL_TRUE);

	if (compiled)
	{
		glAttachShader(program, shader);
		glLinkProgram(program);
		glDetachShader(program, shader);
	}

	glDeleteShader(shader);

	GLint linked = GL_FALSE;
	glGetProgramiv(program, GL_LINK_STATUS, &linked);

	if (!linked)
	{
		GLint length = 0;
		glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);

		std::vector<GLchar> info_log(length);
		glGetProgramInfoLog(program, length, &length, info_log.data());
		
		Log::error("opengl::Driver::createShaderFromSource(): linkage failed, path: \"%s\", error: %s\n", path, info_log.data());
	}

	Shader *result = new Shader();

	result->type = shader_type;
	result->id = program;
	result->compiled = compiled && linked;

	return result;
	/**/
}

backend::Shader *Driver::createShaderFromBytecode(
	ShaderType type,
	uint32_t size,
	const void *data
)
{
	Log::fatal("opengl::Driver::createShaderFromBytecode(): not supported in OpenGL\n");
	return nullptr;
}

//
backend::BindSet *Driver::createBindSet(
)
{
	// TODO: implement
	return nullptr;
}

//
backend::SwapChain *Driver::createSwapChain(
	void *native_window,
	uint32_t width,
	uint32_t height
)
{
	assert(native_window);
	return nullptr;

	// TODO: implement
	/*
	GLint gl_samples = Utils::getSampleCount(num_samples);

	SwapChain *result = new SwapChain();
	result->surface = Platform::createSurface(native_window, gl_samples, request_debug_context);
	result->debug = request_debug_context;

	return result;
	/**/
}

//
void Driver::destroyVertexBuffer(backend::VertexBuffer *vertex_buffer)
{
	if (vertex_buffer == nullptr)
		return;

	VertexBuffer *gl_buffer = static_cast<VertexBuffer *>(vertex_buffer);
	Utils::destroyBuffer(gl_buffer->data);

	glDeleteVertexArrays(1, &gl_buffer->vao_id);
	gl_buffer->vao_id = 0;

	delete gl_buffer;
}

void Driver::destroyIndexBuffer(backend::IndexBuffer *index_buffer)
{
	if (index_buffer == nullptr)
		return;

	IndexBuffer *gl_buffer = static_cast<IndexBuffer *>(index_buffer);
	Utils::destroyBuffer(gl_buffer->data);

	delete gl_buffer;
}

void Driver::destroyTexture(backend::Texture *texture)
{
	if (texture == nullptr)
		return;

	Texture *gl_texture = static_cast<Texture *>(texture);

	glDeleteTextures(1, &gl_texture->id);
	gl_texture->id = 0;

	delete gl_texture;
}

void Driver::destroyFrameBuffer(backend::FrameBuffer *frame_buffer)
{
	if (frame_buffer == nullptr)
		return;

	FrameBuffer *gl_buffer = static_cast<FrameBuffer *>(frame_buffer);

	glDeleteFramebuffers(1, &gl_buffer->id);
	gl_buffer->id = 0;

	delete gl_buffer;
}

void Driver::destroyCommandBuffer(backend::CommandBuffer *command_buffer)
{
	// TODO: implement
}

void Driver::destroyUniformBuffer(backend::UniformBuffer *uniform_buffer)
{
	if (uniform_buffer == nullptr)
		return;

	UniformBuffer *gl_buffer = static_cast<UniformBuffer *>(uniform_buffer);
	Utils::destroyBuffer(gl_buffer->data);

	delete gl_buffer;
}

void Driver::destroyShader(backend::Shader *shader)
{
	if (shader == nullptr)
		return;

	Shader *gl_shader = static_cast<Shader *>(shader);

	glDeleteProgram(gl_shader->id);
	gl_shader->id = 0;

	delete gl_shader;
}

void Driver::destroyBindSet(backend::BindSet *bind_set)
{
	// TODO: implement
}

void Driver::destroySwapChain(backend::SwapChain *swap_chain)
{
	if (swap_chain == nullptr)
		return;

	SwapChain *gl_swap_chain = static_cast<SwapChain *>(swap_chain);

	Platform::destroySurface(gl_swap_chain->surface);
	gl_swap_chain->surface = nullptr;

	delete gl_swap_chain;
}

//
Multisample Driver::getMaxSampleCount()
{
	// TODO: implement
	return Multisample::COUNT_1;
}

Format Driver::getOptimalDepthFormat()
{
	// TODO: implement
	return Format::UNDEFINED;
}

Format Driver::getSwapChainImageFormat(const backend::SwapChain *swap_chain)
{
	// TODO: implement
	return Format::UNDEFINED;
}

uint32_t Driver::getNumSwapChainImages(const backend::SwapChain *swap_chain)
{
	// TODO: implement
	return 0;
}

void Driver::setTextureSamplerWrapMode(backend::Texture *texture, SamplerWrapMode mode)
{
	assert(texture);
	Texture *gl_texture = static_cast<Texture *>(texture);

	GLint wrap_mode = Utils::getTextureWrapMode(mode);
	glTexParameteri(gl_texture->type, GL_TEXTURE_WRAP_S, wrap_mode);
	glTexParameteri(gl_texture->type, GL_TEXTURE_WRAP_T, wrap_mode);
	glTexParameteri(gl_texture->type, GL_TEXTURE_WRAP_R, wrap_mode);
}

void Driver::setTextureSamplerDepthCompare(backend::Texture *texture, bool enabled, DepthCompareFunc func)
{
	assert(texture);
	Texture *gl_texture = static_cast<Texture *>(texture);

	glTexParameteri(gl_texture->type, GL_TEXTURE_COMPARE_MODE, (enabled) ? GL_COMPARE_REF_TO_TEXTURE : GL_NONE);
	glTexParameteri(gl_texture->type, GL_TEXTURE_COMPARE_FUNC, Utils::getDepthCompareFunc(func));
}

void Driver::generateTexture2DMipmaps(backend::Texture *texture)
{
	assert(texture);
	Texture *gl_texture = static_cast<Texture *>(texture);

	glBindTexture(gl_texture->type, gl_texture->id);
	glGenerateMipmap(gl_texture->type);
}

//
void *Driver::map(backend::VertexBuffer *buffer)
{
	assert(buffer);
	VertexBuffer *gl_buffer = static_cast<VertexBuffer *>(buffer);

	glBindBuffer(GL_UNIFORM_BUFFER, gl_buffer->data->id);
	return glMapBufferRange(GL_UNIFORM_BUFFER, 0, gl_buffer->data->size, GL_MAP_WRITE_BIT);
}

void Driver::unmap(backend::VertexBuffer *buffer)
{
	assert(buffer);
	VertexBuffer *gl_buffer = static_cast<VertexBuffer *>(buffer);

	glBindBuffer(GL_UNIFORM_BUFFER, gl_buffer->data->id);
	glUnmapBuffer(GL_UNIFORM_BUFFER);
}

void *Driver::map(backend::IndexBuffer *buffer)
{
	assert(buffer);
	IndexBuffer *gl_buffer = static_cast<IndexBuffer *>(buffer);

	glBindBuffer(GL_UNIFORM_BUFFER, gl_buffer->data->id);
	return glMapBufferRange(GL_UNIFORM_BUFFER, 0, gl_buffer->data->size, GL_MAP_WRITE_BIT);
}

void Driver::unmap(backend::IndexBuffer *buffer)
{
	assert(buffer);
	IndexBuffer *gl_buffer = static_cast<IndexBuffer *>(buffer);

	glBindBuffer(GL_UNIFORM_BUFFER, gl_buffer->data->id);
	glUnmapBuffer(GL_UNIFORM_BUFFER);
}

void *Driver::map(backend::UniformBuffer *buffer)
{
	assert(buffer);
	UniformBuffer *gl_buffer = static_cast<UniformBuffer *>(buffer);

	glBindBuffer(GL_UNIFORM_BUFFER, gl_buffer->data->id);
	return glMapBufferRange(GL_UNIFORM_BUFFER, 0, gl_buffer->data->size, GL_MAP_WRITE_BIT);
}

void Driver::unmap(backend::UniformBuffer *buffer)
{
	assert(buffer);
	UniformBuffer *gl_buffer = static_cast<UniformBuffer *>(buffer);

	glBindBuffer(GL_UNIFORM_BUFFER, gl_buffer->data->id);
	glUnmapBuffer(GL_UNIFORM_BUFFER);
}

//
void Driver::makeCurrent(backend::SwapChain *swap_chain)
{
	assert(swap_chain);
	SwapChain *gl_swap_chain = static_cast<SwapChain *>(swap_chain);

	Platform::makeCurrent(gl_swap_chain->surface);

	if (gl_swap_chain->debug)
	{
		glEnable(GL_DEBUG_OUTPUT);
		glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
		glDebugMessageCallback(debugLog, nullptr);
	}
}

bool Driver::acquire(
	backend::SwapChain *swap_chain,
	uint32_t *new_image
)
{
	assert(swap_chain);
	SwapChain *gl_swap_chain = static_cast<SwapChain *>(swap_chain);

	// TODO: implement
	return false;
}

bool Driver::present(
	backend::SwapChain *swap_chain,
	uint32_t num_wait_command_buffers,
	backend::CommandBuffer * const *wait_command_buffers
)
{
	assert(swap_chain);
	SwapChain *gl_swap_chain = static_cast<SwapChain *>(swap_chain);

	// TODO: implement
	return false;

	// Platform::swapBuffers(gl_swap_chain->surface);
}

bool Driver::wait(
	uint32_t num_wait_command_buffers,
	backend::CommandBuffer * const *wait_command_buffers
)
{
	// TODO: implement
	return false;
}

void Driver::wait()
{
	glFinish();
}

//
void Driver::bindUniformBuffer(
	backend::BindSet *bind_set,
	uint32_t binding,
	const backend::UniformBuffer *uniform_buffer
)
{
	// TODO: implement

	/* set
	assert(binding < MAX_UNIFORM_BUFFERS);

	const UniformBuffer *gl_buffer = static_cast<const UniformBuffer *>(buffer);

	GLuint id = (gl_buffer) ? gl_buffer->data->id : GL_NONE;

	if (pipeline_state.bound_uniform_buffers[binding] != id)
	{
		pipeline_state.bound_uniform_buffers[binding] = id;
		pipeline_state_overrides.bound_uniform_buffers |= (1 << binding);
		pipeline_dirty = true;
	}
	/**/

	/* clear all
	for (uint16_t i = 0; i < MAX_UNIFORM_BUFFERS; ++i)
	{
		GLuint id = pipeline_state.bound_uniform_buffers[i];
		if (id == 0)
			continue;

		pipeline_state.bound_uniform_buffers[i] = 0;
		pipeline_state_overrides.bound_uniform_buffers |= (1 << i);
		pipeline_dirty = true;
	}
	/**/
}

void Driver::bindTexture(
	backend::BindSet *bind_set,
	uint32_t binding,
	const backend::Texture *texture
)
{
	if (bind_set == nullptr)
		return;

	const Texture *gl_texture = static_cast<const Texture *>(texture);

	uint32_t num_mipmaps = (gl_texture) ? gl_texture->mips : 0;
	uint32_t num_layers = (gl_texture) ? gl_texture->layers : 0;

	bindTexture(bind_set, binding, texture, 0, num_mipmaps, 0, num_layers);
}

void Driver::bindTexture(
	backend::BindSet *bind_set,
	uint32_t binding,
	const backend::Texture *texture,
	uint32_t base_mip,
	uint32_t num_mips,
	uint32_t base_layer,
	uint32_t num_layers
)
{
	// TODO: implement

	/* set
	assert(binding < MAX_TEXTURES);

	const Texture *gl_texture = static_cast<const Texture *>(texture);

	GLuint id = (gl_texture) ? gl_texture->id : GL_NONE;
	GLenum type = (gl_texture) ? gl_texture->type : GL_NONE;

	if (pipeline_state.bound_textures[binding] != id || pipeline_state.bound_texture_types[binding] != type)
	{
		pipeline_state.bound_textures[binding] = id;
		pipeline_state.bound_texture_types[binding] = type;
		pipeline_state_overrides.bound_textures |= (1 << binding);
		pipeline_dirty = true;
	}
	/**/

	/* clear all
	for (uint16_t i = 0; i < MAX_TEXTURES; ++i)
	{
		GLuint id = pipeline_state.bound_textures[i];
		if (id == 0)
			continue;

		pipeline_state.bound_textures[i] = 0;
		pipeline_state_overrides.bound_textures |= (1 << i);
		pipeline_dirty = true;
	}
	/**/
}

//
void Driver::clearPushConstants(
)
{
	// TODO: implement
}

void Driver::setPushConstants(
	uint8_t size,
	const void *data
)
{
	// TODO: implement
}

void Driver::clearBindSets(
)
{
	// TODO: implement
}

void Driver::allocateBindSets(
	uint8_t size
)
{
	// TODO: implement
}

void Driver::pushBindSet(
	backend::BindSet *bind_set
)
{
	// TODO: implement
}

void Driver::setBindSet(
	uint32_t binding,
	backend::BindSet *bind_set
)
{
	// TODO: implement
}

void Driver::clearShaders(
)
{
	// TODO: implement

	/*
	for (uint16_t i = 0; i < MAX_SHADERS; ++i)
	{
		GLuint id = pipeline_state.bound_shaders[i];
		if (id == 0)
			continue;

		pipeline_state.bound_shaders[i] = 0;
		pipeline_state_overrides.bound_shaders |= (1 << i);
		pipeline_dirty = true;
	}
	/**/
}

void Driver::setShader(
	ShaderType type,
	const backend::Shader *shader
)
{
	// TODO: implement

	/*
	const Shader *gl_shader = static_cast<const Shader *>(shader);

	if (gl_shader && gl_shader->type != Utils::getShaderType(type))
	{
		Log::error("opengl::Driver::bindShader(): shader type mismatch\n");
		return;
	}

	uint8_t index = static_cast<uint8_t>(type);
	GLuint id = (gl_shader) ? gl_shader->id : GL_NONE;

	if (pipeline_state.bound_shaders[index] != id)
	{
		pipeline_state.bound_shaders[index] = id;
		pipeline_state_overrides.bound_shaders |= (1 << index);
		pipeline_dirty = true;
	}
	/**/
}

//
void Driver::setViewport(
	float x,
	float y,
	float width,
	float height
)
{
	// TODO: implement

	/*
	if (pipeline_state.viewport_x != x)
	{
		pipeline_state.viewport_x = x;
		pipeline_state_overrides.viewport = 1;
		pipeline_dirty = true;
	}

	if (pipeline_state.viewport_y != y)
	{
		pipeline_state.viewport_y = y;
		pipeline_state_overrides.viewport = 1;
		pipeline_dirty = true;
	}

	if (pipeline_state.viewport_width != width)
	{
		pipeline_state.viewport_width = width;
		pipeline_state_overrides.viewport = 1;
		pipeline_dirty = true;
	}

	if (pipeline_state.viewport_height != height)
	{
		pipeline_state.viewport_height = height;
		pipeline_state_overrides.viewport = 1;
		pipeline_dirty = true;
	}
	/**/
}

void Driver::setScissor(
	int32_t x,
	int32_t y,
	uint32_t width,
	uint32_t height
)
{
	// TODO: implement
}

void Driver::setCullMode(
	CullMode mode
)
{
	// TODO: implement

	/*
	GLenum cull_mode = Utils::getCullMode(mode);

	if (pipeline_state.cull_mode != cull_mode)
	{
		pipeline_state.cull_mode = cull_mode;
		pipeline_state_overrides.cull_mode = 1;
		pipeline_dirty = true;
	}
	/**/
}

void Driver::setDepthTest(
	bool enabled
)
{
	// TODO: implement

	/*
	if (pipeline_state.depth_test != enabled)
	{
		pipeline_state.depth_test = enabled;
		pipeline_state_overrides.depth_test = 1;
		pipeline_dirty = true;
	}
	/**/
}

void Driver::setDepthWrite(
	bool enabled
)
{
	// TODO: implement

	/*
	if (pipeline_state.depth_write != enabled)
	{
		pipeline_state.depth_write = enabled;
		pipeline_state_overrides.depth_write = 1;
		pipeline_dirty = true;
	}
	/**/
}

void Driver::setDepthCompareFunc(
	DepthCompareFunc func
)
{
	// TODO: implement

	/*
	GLenum gl_func = Utils::getDepthCompareFunc(func);

	if (pipeline_state.depth_comparison_func != gl_func)
	{
		pipeline_state.depth_comparison_func = gl_func;
		pipeline_state_overrides.depth_comparison_func = 1;
		pipeline_dirty = true;
	}
	/**/
}

void Driver::setBlending(
	bool enabled
)
{
	// TODO: implement

	/*
	if (pipeline_state.blend != enabled)
	{
		pipeline_state.blend = enabled;
		pipeline_state_overrides.blend = 1;
		pipeline_dirty = true;
	}
	/**/
}

void Driver::setBlendFactors(
	BlendFactor src_factor,
	BlendFactor dest_factor
)
{
	// TODO: implement

	/*
	GLenum gl_src_factor = Utils::getBlendFactor(src_factor);
	GLenum gl_dst_factor = Utils::getBlendFactor(dest_factor);

	if (pipeline_state.blend_src_factor != gl_src_factor || pipeline_state.blend_dst_factor != gl_dst_factor)
	{
		pipeline_state.blend_src_factor = gl_src_factor;
		pipeline_state.blend_dst_factor = gl_dst_factor;
		pipeline_state_overrides.blend_factors = 1;
		pipeline_dirty = true;
	}
	/**/
}

//
bool Driver::resetCommandBuffer(
	backend::CommandBuffer *command_buffer
)
{
	// TODO: implement
	return false;
}

bool Driver::beginCommandBuffer(
	backend::CommandBuffer *command_buffer
)
{
	// TODO: implement
	return false;
}

bool Driver::endCommandBuffer(
	backend::CommandBuffer *command_buffer
)
{
	// TODO: implement
	return false;
}

bool Driver::submit(
	backend::CommandBuffer *command_buffer
)
{
	// TODO: implement
	return false;
}

bool Driver::submitSyncked(
	backend::CommandBuffer *command_buffer,
	const backend::SwapChain *wait_swap_chain
)
{
	// TODO: implement
	return false;
}

bool Driver::submitSyncked(
	backend::CommandBuffer *command_buffer,
	uint32_t num_wait_command_buffers,
	backend::CommandBuffer * const *wait_command_buffers
)
{
	// TODO: implement
	return false;
}

//
void Driver::beginRenderPass(
	backend::CommandBuffer *command_buffer,
	const backend::FrameBuffer *frame_buffer,
	const RenderPassInfo *info
)
{
	// TODO: implement

	/*
	const FrameBuffer *gl_buffer = static_cast<const FrameBuffer *>(frame_buffer);

	if (!gl_buffer)
	{
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		return;
	}

	glBindFramebuffer(GL_FRAMEBUFFER, gl_buffer->id);

	if (!clear)
		return;

	GLfloat clear_color[4] = {0.0f, 0.0f, 0.0f, 1.0f};
	for (uint8_t i = 0; i < gl_buffer->num_color_attachments; ++i)
		glClearBufferfv(GL_COLOR, i, clear_color);

	if (gl_buffer->depthstencil_attachment.id != GL_NONE)
		glClearBufferfi(GL_DEPTH_STENCIL, 0, 1.0f, 0);
	/**/

	/* clear color
	glClearColor(r, g, b, a);
	glClear(GL_COLOR_BUFFER_BIT);
	/**/

	/* clear depth/stencil
	glClearDepthf(depth);
	glClearStencil(stencil);
	glClear(GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
	/**/


}

void Driver::beginRenderPass(
	backend::CommandBuffer *command_buffer,
	const backend::SwapChain *swap_chain,
	const RenderPassInfo *info
)
{
	// TODO: implement

	/*
	const FrameBuffer *gl_buffer = static_cast<const FrameBuffer *>(frame_buffer);

	if (!gl_buffer)
	{
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		return;
	}

	glBindFramebuffer(GL_FRAMEBUFFER, gl_buffer->id);

	if (!clear)
		return;

	GLfloat clear_color[4] = {0.0f, 0.0f, 0.0f, 1.0f};
	for (uint8_t i = 0; i < gl_buffer->num_color_attachments; ++i)
		glClearBufferfv(GL_COLOR, i, clear_color);

	if (gl_buffer->depthstencil_attachment.id != GL_NONE)
		glClearBufferfi(GL_DEPTH_STENCIL, 0, 1.0f, 0);
	/**/

	/* clear color
	glClearColor(r, g, b, a);
	glClear(GL_COLOR_BUFFER_BIT);
	/**/

	/* clear depth/stencil
	glClearDepthf(depth);
	glClearStencil(stencil);
	glClear(GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
	/**/
}

void Driver::endRenderPass(
	backend::CommandBuffer *command_buffer
)
{
	// TODO: implement
	// glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Driver::drawIndexedPrimitive(
	backend::CommandBuffer *command_buffer,
	const RenderPrimitive *render_primitive
)
{
	// TODO: implement

	/* NOTE: probably save draw call info into a struct and store it in the command buffer

	struct DrawCall
	{
		PipelineState state;
		GLint vao;
		GLint io;
		RenderPrimitive render_primitive;
	};
	/**/

	/*
	assert(render_primitive);
	flushPipelineState();

	const VertexBuffer *vb = static_cast<const VertexBuffer *>(render_primitive->vertices);
	const IndexBuffer *ib = static_cast<const IndexBuffer *>(render_primitive->indices);

	if (bound_vao != vb->vao_id)
	{
		bound_vao = vb->vao_id;
		glBindVertexArray(vb->vao_id);
	}
	
	if (bound_ib != ib->data->id)
	{
		bound_ib = ib->data->id;
		glBindBuffer(ib->data->type, ib->data->id);
	}

	glDrawElementsInstancedBaseVertex(
		Utils::getPrimitiveType(render_primitive->type),
		ib->num_indices,
		ib->index_format,
		static_cast<const char *>(0) + render_primitive->base_index,
		num_instances,
		render_primitive->vertex_index_offset
	);
	/**/
}
}