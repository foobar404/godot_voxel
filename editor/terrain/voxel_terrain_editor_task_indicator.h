#ifndef VOXEL_TERRAIN_EDITOR_TASK_INDICATOR_H
#define VOXEL_TERRAIN_EDITOR_TASK_INDICATOR_H

#include "../../server/voxel_server.h"

#include <editor/editor_scale.h>
#include <scene/gui/box_container.h>
#include <scene/gui/label.h>
#include <scene/gui/separator.h>

namespace zylann::voxel {

class VoxelTerrainEditorTaskIndicator : public HBoxContainer {
	GDCLASS(VoxelTerrainEditorTaskIndicator, HBoxContainer)
private:
	enum StatID {
		STAT_STREAM_TASKS,
		STAT_GENERATE_TASKS,
		STAT_MESH_TASKS,
		STAT_MAIN_THREAD_TASKS,
		STAT_MEMORY,
		STAT_COUNT
	};

public:
	VoxelTerrainEditorTaskIndicator() {
		create_stat(STAT_STREAM_TASKS, TTR("Streaming"), TTR("Streaming tasks"));
		create_stat(STAT_GENERATE_TASKS, TTR("Generation"), TTR("Generation tasks"));
		create_stat(STAT_MESH_TASKS, TTR("Meshing"), TTR("Meshing tasks"));
		create_stat(STAT_MAIN_THREAD_TASKS, TTR("Main"), TTR("Main thread tasks"));
		create_stat(STAT_MEMORY, TTR("Memory"), TTR("Memory usage (whole editor, not just voxel)"));
	}

	void _notification(int p_what) {
		switch (p_what) {
			case NOTIFICATION_THEME_CHANGED:
				// Set a monospace font.
				// Can't do this in constructor, fonts are not available then. Also the theme can change.
				for (unsigned int i = 0; i < _stats.size(); ++i) {
					_stats[i].label->add_theme_font_override("font", get_theme_font("source", "EditorFonts"));
				}
				break;
		}
	}

	void update_stats() {
		const VoxelServer::Stats stats = VoxelServer::get_singleton()->get_stats();
		set_stat(STAT_STREAM_TASKS, stats.streaming_tasks);
		set_stat(STAT_GENERATE_TASKS, stats.generation_tasks);
		set_stat(STAT_MESH_TASKS, stats.meshing_tasks);
		set_stat(STAT_MAIN_THREAD_TASKS, stats.main_thread_tasks);
		set_stat(STAT_MEMORY, int64_t(OS::get_singleton()->get_static_memory_usage()), "b");
	}

private:
	void create_stat(StatID id, String short_name, String long_name) {
		add_child(memnew(VSeparator));
		Stat &stat = _stats[id];
		CRASH_COND(stat.label != nullptr);
		Label *name_label = memnew(Label);
		name_label->set_text(short_name);
		name_label->set_tooltip(long_name);
		name_label->set_mouse_filter(Control::MOUSE_FILTER_PASS); // Necessary for tooltip to work
		add_child(name_label);
		stat.label = memnew(Label);
		stat.label->set_custom_minimum_size(Vector2(60 * EDSCALE, 0));
		stat.label->set_text("---");
		add_child(stat.label);
	}

	static String with_commas(int64_t n) {
		String res = "";
		if (n < 0) {
			res += "-";
			n = -n;
		}
		String s = String::num_int64(n);
		const int mod = s.length() % 3;
		for (int i = 0; i < s.length(); ++i) {
			if (i != 0 && i % 3 == mod) {
				res += ",";
			}
			res += s[i];
		}
		return res;
	}

	// There is `String::humanize_size` but:
	//1) it is specifically for bytes, 2) it works on a 1024 base
	// This function allows any unit, and works on a base of 1000
	static String with_unit(int64_t n, const char *unit) {
		String s = "";
		if (n < 0) {
			s += "-";
			n = -n;
		}
		// TODO Perhaps use a for loop
		if (n >= 1000'000'000'000) {
			s += with_commas(n / 1000'000'000'000);
			s += ".";
			s += String::num_int64((n % 1000'000'000'000) / 1000'000'000).pad_zeros(3);
			s += " T";
			s += unit;
			return s;
		}
		if (n >= 1000'000'000) {
			s += String::num_int64(n / 1000'000'000);
			s += ".";
			s += String::num_int64((n % 1000'000'000) / 1000'000).pad_zeros(3);
			s += " G";
			s += unit;
			return s;
		}
		if (n >= 1000'000) {
			s += String::num_int64(n / 1000'000);
			s += ".";
			s += String::num_int64((n % 1000'000) / 1000).pad_zeros(3);
			s += " M";
			s += unit;
			return s;
		}
		if (n >= 1000) {
			s += String::num_int64(n / 1000);
			s += ".";
			s += String::num_int64(n % 1000).pad_zeros(3);
			s += " K";
			s += unit;
			return s;
		}
		s += String::num_int64(n);
		s += " ";
		s += unit;
		return s;
	}

	void set_stat(StatID id, int64_t value) {
		Stat &stat = _stats[id];
		if (stat.value != value) {
			stat.value = value;
			stat.label->set_text(with_commas(stat.value));
		}
	}

	void set_stat(StatID id, int64_t value, const char *unit) {
		Stat &stat = _stats[id];
		if (stat.value != value) {
			stat.value = value;
			stat.label->set_text(with_unit(stat.value, unit));
		}
	}

	struct Stat {
		int64_t value = 0;
		Label *label = nullptr;
	};

	FixedArray<Stat, STAT_COUNT> _stats;
};

} // namespace zylann::voxel

#endif // VOXEL_TERRAIN_EDITOR_TASK_INDICATOR_H
