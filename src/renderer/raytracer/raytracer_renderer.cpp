#include "raytracer_renderer.h"

#include "utils/resource_utils.h"

#include <iostream>


void cg::renderer::ray_tracing_renderer::init()
{
	raytracer = std::make_shared<cg::renderer::raytracer<cg::vertex, cg::unsigned_color>>();
	raytracer->set_viewport(settings->width, settings->height);

	render_target = std::make_shared<cg::resource<cg::unsigned_color>>(settings->width, settings->height);

	raytracer->set_render_target(render_target);
	
	model = std::make_shared<cg::world::model>();
	model->load_obj(settings->model_path);

	camera = std::make_shared<cg::world::camera>();
	camera->set_height(static_cast<float>(settings->height));
	camera->set_width(static_cast<float>(settings->width));
	camera->set_position(float3{settings->camera_position[0], settings->camera_position[1], settings->camera_position[2]});
	camera->set_phi(settings->camera_phi);
	camera->set_theta(settings->camera_theta);
	camera->set_angle_of_view(settings->camera_angle_of_view);
	camera->set_z_near(settings->camera_z_near);
	camera->set_z_far(settings->camera_z_far);

	raytracer->set_vertex_buffers(model->get_vertex_buffers());
	raytracer->set_index_buffers(model->get_index_buffers());

	lights.push_back({float3{-0.24f, 1.97f, 0.16f}, float3{0.78f, 0.78f, 0.78f}/4.f});
	lights.push_back({float3{-0.24f, 1.97f, -0.22f}, float3{0.78f, 0.78f, 0.78f}/4.f});
	lights.push_back({float3{0.23f, 1.97f, -0.22f}, float3{0.78f, 0.78f, 0.78f}/4.f});
	lights.push_back({float3{0.23f, 1.97f, 0.16f}, float3{0.78f, 0.78f, 0.78f}/4.f});

	shadow_raytracer = std::make_shared<cg::renderer::raytracer<cg::vertex, cg::unsigned_color>>();
	shadow_raytracer->set_vertex_buffers(model->get_vertex_buffers());
	shadow_raytracer->set_index_buffers(model->get_index_buffers());
}

void cg::renderer::ray_tracing_renderer::destroy() {}

void cg::renderer::ray_tracing_renderer::update() {}

void cg::renderer::ray_tracing_renderer::render()
{
	raytracer->clear_render_target({0, 0, 0});
	raytracer->build_acceleration_structure();
	raytracer->miss_shader = [](const ray& ray) {payload payload{}; payload.color = {0.f, 0.f, 0.f}; return payload;};

	std::random_device random_device;
	std::mt19937 random_generator(random_device());
	std::uniform_real_distribution<float> uniform_dist(-1.f, 1.f);

	raytracer->closest_hit_shader = [&](const ray& ray, payload& payload, const triangle<cg::vertex>& triangle, size_t depth) {

		float3 position = ray.position + ray.direction * payload.t;
		float3 normal = normalize(
				payload.bary.x * triangle.na +
				payload.bary.y * triangle.nb +
				payload.bary.z * triangle.nc);
		float3 result_color = triangle.emissive;

		for (auto & light : lights) {
			cg::renderer::ray to_light(position, light.position - position);
			result_color += triangle.diffuse * light.color * std::max(dot(normal, to_light.direction), 0.f);
		}

		float3 random_direction {
				uniform_dist(random_generator),
				uniform_dist(random_generator),
				uniform_dist(random_generator),
		};

		if (dot(normal, random_direction) < 0.f) {
			random_direction = -random_direction;
		}

		cg::renderer::ray to_next_object(position, random_direction);

		auto playload_next = raytracer->trace_ray(to_next_object, depth);

		result_color += triangle.diffuse * playload_next.color.to_float3() * std::max(dot(normal, to_next_object.direction), 0.f);

		payload.color = cg::color::from_float3(result_color);

		return payload;
	};

	shadow_raytracer->acceleration_structures = raytracer->acceleration_structures;
	shadow_raytracer->miss_shader = [](const ray& ray) {
		payload payload{};
		payload.t = -1.f;
		return payload;
	};
    shadow_raytracer->any_hit_shader = [](const ray& ray, payload& payload, const triangle<cg::vertex>& triangle) {
		return payload;
	};
	
	auto start = std::chrono::high_resolution_clock::now();

	raytracer->ray_generation(camera->get_position(), camera->get_direction(), camera->get_right(), camera->get_up(), settings->raytracing_depth, settings->accumulation_num);

	auto stop = std::chrono::high_resolution_clock::now();

	std::chrono::duration<float, std::milli> rt_duration = stop - start;

	std::cout << "Ray tracing took: " << rt_duration.count() << " ms \n";
	
	cg::utils::save_resource(*render_target, settings->result_path);
}