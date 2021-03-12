#version 450
#pragma shader_stage(vertex)

#include <common/RenderState.inc>

layout(push_constant) uniform RenderNodeState {
	mat4 transform;
} node;

// Input
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inTangent;
layout(location = 2) in vec3 inBinormal;
layout(location = 3) in vec3 inNormal;
layout(location = 4) in vec3 inColor;
layout(location = 5) in vec2 inTexCoord;

// Output
layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragTexCoord;
layout(location = 2) out vec3 fragTangentVS;
layout(location = 3) out vec3 fragBinormalVS;
layout(location = 4) out vec3 fragNormalVS;
layout(location = 5) out vec3 fragPositionVS;
layout(location = 6) out vec4 fragPositionNdc;
layout(location = 7) out vec4 fragPositionNdcOld;

void main() {
	mat4 modelview = ubo.view * node.transform;
	mat4 modelviewOld = ubo.viewOld * node.transform; // TODO: old node transform

	gl_Position = ubo.projection * modelview * vec4(inPosition, 1.0f);

	fragColor = inColor;
	fragTexCoord = inTexCoord;

	fragTangentVS = vec3(modelview * vec4(inTangent, 0.0f));
	fragBinormalVS = vec3(modelview * vec4(inBinormal, 0.0f));
	fragNormalVS = vec3(modelview * vec4(inNormal, 0.0f));
	fragPositionVS = vec3(modelview * vec4(inPosition, 1.0f));
	fragPositionNdc = vec4(ubo.projection * modelview * vec4(inPosition, 1.0f));
	fragPositionNdcOld = vec4(ubo.projection * modelviewOld * vec4(inPosition, 1.0f));
}
