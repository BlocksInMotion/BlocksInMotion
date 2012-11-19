
#include "a2e_cl.h"
#include "a2e_cl_particle_init.h"

// this is used for both the initial particle spawning and the particle respawning

#ifdef A2E_PARTICLE_INIT
kernel void particle_init
#else
kernel void particle_respawn
#endif
				(const uint type, const float living_time, const uint particle_count, const float velocity, const float4 angle, const float4 extents, const float4 direction,
				 const float4 position_offset, const uint seed,
#ifdef A2E_PARTICLE_INIT
				 const float spawn_rate_ts,
#else
				 const float4 gravity,
#endif
				 global float4* pos_time_buffer, global float4* dir_buffer) {
	uint particle_num = get_global_id(0);
	if(particle_num >= particle_count) return;
	
	// init this random seed (this _must_ by different for each particle, also offset particle number by seed, so we don't multiply 16807 by small numbers)
	uint random_seed = seed ^ mul24(seed - particle_num, 16807u);
	
#ifndef A2E_PARTICLE_INIT
	float4 pos_time = pos_time_buffer[particle_num];
	//float4 dir_vel = dir_buffer[particle_num];
	float particle_time = pos_time.w;
	if(particle_time > 0.0f) return;
	
	float spawn_rate_ts = 0.0f;
#endif
	
	float4 position, dir;
	init_particles(&position, &dir, &random_seed, type, spawn_rate_ts, living_time, particle_count, velocity, angle, extents, direction, position_offset);
	
#ifndef A2E_PARTICLE_INIT
	float tpassed = -particle_time;
	float time_step = tpassed / 1000.0f;
	float4 acceleration = gravity * time_step;
	dir += acceleration;
	position += dir * time_step;
	
	particle_time += living_time;
	position = (float4)(position.x, position.y, position.z, particle_time);
	dir = (float4)(dir.x, dir.y, dir.z, 0.0f);
#endif

	pos_time_buffer[particle_num] = position;
	dir_buffer[particle_num] = dir;
}
