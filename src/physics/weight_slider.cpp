/*
 *  Blocks In Motion
 *  Copyright (C) 2012 Florian Ziesche
 *  
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "weight_slider.h"
#include "physics_controller.h"
#include "rigid_body.h"

weight_slider::weight_slider(const float3& position_, const float& height_, const float& mass_) :
position(position_), height(height_), mass(mass_)
{
	//
	avg_norm_height.fill(1.0f);
	
	// create slider bodies (base is fixed/static, tray is dynamic)
	constexpr float body_height = 0.025f;
	base_info = &pc->add_rigid_info<physics_controller::SHAPE::BOX>(0.0f, float3(0.5f, body_height, 0.5f));
	tray_info = &pc->add_rigid_info<physics_controller::SHAPE::BOX>(mass, float3(0.5f, body_height, 0.5f));
	base_body = &pc->add_rigid_body(*base_info, position + float3(0.5f, 0.0f, 0.5f));
	tray_body = &pc->add_rigid_body(*tray_info, position + float3(0.5f, height, 0.5f));
	
	//
	btTransform base_transform(btTransform::getIdentity());
	btTransform tray_transform(btTransform::getIdentity());
	base_transform.getBasis().setValue(0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f);
	tray_transform.getBasis().setValue(0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f);
	tray_transform.setOrigin(btVector3(0.0f, height - 0.1f, 0.0f));
	constraint = new btSliderConstraint(*base_body->get_body(), *tray_body->get_body(),
										base_transform, tray_transform, true);
	
	constraint->setLowerLinLimit(0.0f);
	constraint->setUpperLinLimit(height);
	constraint->setPoweredLinMotor(true);
	constraint->setTargetLinMotorVelocity(100.0f);
	constraint->setMaxLinMotorForce(100.0f);
	
	// disable any angular rotation
	constraint->setLowerAngLimit(0.0f);
	constraint->setUpperAngLimit(0.0f);
	constraint->setSoftnessDirAng(0.0f);
	constraint->setRestitutionDirAng(0.0f);
	constraint->setDampingDirAng(0.0f);
	constraint->setSoftnessLimAng(0.0f);
	constraint->setRestitutionLimAng(0.0f);
	constraint->setDampingLimAng(0.0f);
	constraint->setSoftnessOrthoAng(0.0f);
	constraint->setRestitutionOrthoAng(0.0f);
	constraint->setDampingOrthoAng(0.0f);
	constraint->setPoweredAngMotor(false);
	
	// body rotation must also be disabled (+disable deactivation)
	base_body->get_body()->setActivationState(DISABLE_DEACTIVATION);
	base_body->get_body()->setSleepingThresholds(0.0f, 0.0f);
	base_body->get_body()->setAngularFactor(0.0f);
	tray_body->get_body()->setActivationState(DISABLE_DEACTIVATION);
	tray_body->get_body()->setSleepingThresholds(0.0f, 0.0f);
	tray_body->get_body()->setAngularFactor(0.0f);
}

weight_slider::~weight_slider() {
	pc->remove_rigid_info(base_info);
	pc->remove_rigid_info(tray_info);
	pc->remove_rigid_body(base_body);
	pc->remove_rigid_body(tray_body);
	delete constraint;
}

rigid_body* weight_slider::get_base_body() {
	return base_body;
}

rigid_body* weight_slider::get_tray_body() {
	return tray_body;
}

btSliderConstraint* weight_slider::get_constraint() {
	return constraint;
}

float weight_slider::get_height() const {
	return constraint->getLinearPos();
}

float weight_slider::get_normalized_height() const {
	return core::clamp(get_height() / height, 0.0f, 1.0f);
}

float weight_slider::get_render_height() const {
	return get_normalized_height() * 0.2f;
}

bool weight_slider::is_triggered() const {
	const float avg(get_avg_normalized_height());
	return (avg > 0.01f && avg < 0.925f);
}

void weight_slider::update() {
	avg_norm_height[cur_avg_pos++] = get_normalized_height();
	cur_avg_pos %= avg_norm_height.size();
}

float weight_slider::get_avg_normalized_height() const {
	float avg = 0.0f;
	for(const float& nheight : avg_norm_height) avg += nheight;
	return (avg / float(avg_norm_height.size()));
}
