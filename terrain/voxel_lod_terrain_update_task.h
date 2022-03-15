#ifndef VOXEL_LOD_TERRAIN_UPDATE_TASK_H
#define VOXEL_LOD_TERRAIN_UPDATE_TASK_H

#include "../server/priority_dependency.h"
#include "../util/tasks/threaded_task.h"
#include "voxel_lod_terrain_update_data.h"

namespace zylann::voxel {

struct StreamingDependency;
struct MeshingDependency;

// Runs a part of the update loop of a VoxelLodTerrain.
// This part can run on another thread, so multiple terrains can update in parallel.
// There must be only one running at once per terrain.
// Note, this task does not include meshing and voxel generation. These are done with different tasks.
//
// IMPORTANT: The work done by this task must not involve any call to Godot's servers, directly or indirectly.
// These are deferred to the main thread.
//
class VoxelLodTerrainUpdateTask : public IThreadedTask {
public:
	VoxelLodTerrainUpdateTask(std::shared_ptr<VoxelDataLodMap> p_data,
			std::shared_ptr<VoxelLodTerrainUpdateData> p_update_data,
			std::shared_ptr<StreamingDependency> p_streaming_dependency,
			std::shared_ptr<MeshingDependency> p_meshing_dependency,
			std::shared_ptr<PriorityDependency::ViewersData> p_shared_viewers_data, Vector3 p_viewer_pos,
			bool p_request_instances, uint32_t p_volume_id, Transform3D p_volume_transform) :
			//
			_data(p_data),
			_update_data(p_update_data),
			_streaming_dependency(p_streaming_dependency),
			_meshing_dependency(p_meshing_dependency),
			_shared_viewers_data(p_shared_viewers_data),
			_viewer_pos(p_viewer_pos),
			_request_instances(p_request_instances),
			_volume_id(p_volume_id),
			_volume_transform(p_volume_transform) {}

	void run(ThreadedTaskContext ctx) override;
	void apply_result() override {}

	// Functions also used outside of this task

	static void flush_pending_lod_edits(VoxelLodTerrainUpdateData::State &state, VoxelDataLodMap &data,
			Ref<VoxelGenerator> generator, bool full_load_mode);

	static uint8_t get_transition_mask(
			const VoxelLodTerrainUpdateData::State &state, Vector3i block_pos, int lod_index, unsigned int lod_count);

private:
	std::shared_ptr<VoxelDataLodMap> _data;
	std::shared_ptr<VoxelLodTerrainUpdateData> _update_data;
	std::shared_ptr<StreamingDependency> _streaming_dependency;
	std::shared_ptr<MeshingDependency> _meshing_dependency;
	std::shared_ptr<PriorityDependency::ViewersData> _shared_viewers_data;
	Vector3 _viewer_pos;
	bool _request_instances;
	uint32_t _volume_id;
	Transform3D _volume_transform;
};

} // namespace zylann::voxel

#endif // VOXEL_LOD_TERRAIN_UPDATE_TASK_H
