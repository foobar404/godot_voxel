#include "voxel_graph_editor_plugin.h"
#include "../../generators/graph/voxel_generator_graph.h"
#include "../../terrain/voxel_node.h"
#include "editor_property_text_change_on_submit.h"
#include "voxel_graph_editor.h"
#include "voxel_graph_node_inspector_wrapper.h"

#include <editor/editor_data.h>
#include <editor/editor_scale.h>

namespace zylann::voxel {

// Changes string editors of the inspector to call setters only when enter key is pressed, similar to Unreal.
// Because the default behavior of `EditorPropertyText` is to call the setter on every character typed, which is a
// nightmare when editing an Expression node: inputs change constantly as the code is written which has much higher
// chance of messing up existing connections, and creates individual UndoRedo actions as well.
class VoxelGraphEditorInspectorPlugin : public EditorInspectorPlugin {
	GDCLASS(VoxelGraphEditorInspectorPlugin, EditorInspectorPlugin)
public:
	bool can_handle(Object *obj) override {
		return obj != nullptr && Object::cast_to<VoxelGraphNodeInspectorWrapper>(obj) != nullptr;
	}

	bool parse_property(Object *p_object, const Variant::Type p_type, const String &p_path, const PropertyHint p_hint,
			const String &p_hint_text, const uint32_t p_usage, const bool p_wide = false) override {
		if (p_type == Variant::STRING) {
			add_property_editor(p_path, memnew(EditorPropertyTextChangeOnSubmit));
			return true;
		}
		return false;
	}
};

VoxelGraphEditorPlugin::VoxelGraphEditorPlugin() {
	//EditorInterface *ed = get_editor_interface();
	_graph_editor = memnew(VoxelGraphEditor);
	_graph_editor->set_custom_minimum_size(Size2(0, 300) * EDSCALE);
	_graph_editor->connect(VoxelGraphEditor::SIGNAL_NODE_SELECTED,
			callable_mp(this, &VoxelGraphEditorPlugin::_on_graph_editor_node_selected));
	_graph_editor->connect(VoxelGraphEditor::SIGNAL_NOTHING_SELECTED,
			callable_mp(this, &VoxelGraphEditorPlugin::_on_graph_editor_nothing_selected));
	_graph_editor->connect(VoxelGraphEditor::SIGNAL_NODES_DELETED,
			callable_mp(this, &VoxelGraphEditorPlugin::_on_graph_editor_nodes_deleted));
	_graph_editor->connect(VoxelGraphEditor::SIGNAL_REGENERATE_REQUESTED,
			callable_mp(this, &VoxelGraphEditorPlugin::_on_graph_editor_regenerate_requested));
	_bottom_panel_button = add_control_to_bottom_panel(_graph_editor, TTR("Voxel Graph"));
	_bottom_panel_button->hide();

	Ref<VoxelGraphEditorInspectorPlugin> inspector_plugin;
	inspector_plugin.instantiate();
	add_inspector_plugin(inspector_plugin);
}

bool VoxelGraphEditorPlugin::handles(Object *p_object) const {
	if (p_object == nullptr) {
		return false;
	}
	VoxelGeneratorGraph *graph_ptr = Object::cast_to<VoxelGeneratorGraph>(p_object);
	if (graph_ptr != nullptr) {
		return true;
	}
	VoxelGraphNodeInspectorWrapper *wrapper = Object::cast_to<VoxelGraphNodeInspectorWrapper>(p_object);
	if (wrapper != nullptr) {
		return true;
	}
	return false;
}

void VoxelGraphEditorPlugin::edit(Object *p_object) {
	VoxelGeneratorGraph *graph_ptr = Object::cast_to<VoxelGeneratorGraph>(p_object);
	if (graph_ptr == nullptr) {
		VoxelGraphNodeInspectorWrapper *wrapper = Object::cast_to<VoxelGraphNodeInspectorWrapper>(p_object);
		if (wrapper != nullptr) {
			graph_ptr = *wrapper->get_graph();
		}
	}
	Ref<VoxelGeneratorGraph> graph(graph_ptr);
	_graph_editor->set_undo_redo(&get_undo_redo()); // UndoRedo isn't available in constructor
	_graph_editor->set_graph(graph);

	VoxelNode *voxel_node = nullptr;
	Array selected_nodes = get_editor_interface()->get_selection()->get_selected_nodes();
	for (int i = 0; i < selected_nodes.size(); ++i) {
		Node *node = Object::cast_to<Node>(selected_nodes[i]);
		ERR_FAIL_COND(node == nullptr);
		VoxelNode *vn = Object::cast_to<VoxelNode>(node);
		if (vn != nullptr && vn->get_generator() == graph_ptr) {
			voxel_node = vn;
			break;
		}
	}
	_voxel_node = voxel_node;
	_graph_editor->set_voxel_node(voxel_node);
}

void VoxelGraphEditorPlugin::make_visible(bool visible) {
	if (visible) {
		_bottom_panel_button->show();
		make_bottom_panel_item_visible(_graph_editor);

	} else {
		_voxel_node = nullptr;
		_graph_editor->set_voxel_node(nullptr);

		if (!_graph_editor->is_pinned_hint()) {
			_bottom_panel_button->hide();

			// TODO Awful hack to handle the nonsense happening in `_on_graph_editor_node_selected`
			if (!_deferred_visibility_scheduled) {
				_deferred_visibility_scheduled = true;
				call_deferred("_hide_deferred");
			}
		}
	}
}

void VoxelGraphEditorPlugin::_hide_deferred() {
	_deferred_visibility_scheduled = false;
	if (_bottom_panel_button->is_visible()) {
		// Still visible actually? Don't hide then
		return;
	}
	// The point is when the plugin's UI closed (for real, not closed and re-opened simultaneously!),
	// it should cleanup its UI to not waste RAM (as it references stuff).
	edit(nullptr);
	if (_graph_editor->is_visible_in_tree()) {
		hide_bottom_panel();
	}
}

void VoxelGraphEditorPlugin::_on_graph_editor_node_selected(uint32_t node_id) {
	Ref<VoxelGraphNodeInspectorWrapper> wrapper;
	wrapper.instantiate();
	wrapper->setup(_graph_editor->get_graph(), node_id, &get_undo_redo(), _graph_editor);
	// Note: it's neither explicit nor documented, but the reference will stay alive due to EditorHistory::_add_object
	get_editor_interface()->inspect_object(*wrapper);
	// TODO Absurd situation here...
	// `inspect_object()` gets to a point where Godot hides ALL plugins for some reason...
	// And all this, to have the graph editor rebuilt and shown again, because it DOES also handle that resource type
	// -_- https://github.com/godotengine/godot/issues/40166
}

void VoxelGraphEditorPlugin::_on_graph_editor_nothing_selected() {
	// The inspector is a glorious singleton, so when we select nodes to edit their properties, it prevents
	// from accessing the graph resource itself, to save it for example.
	// I'd like to embed properties inside the nodes themselves, but it's a bit more work,
	// so for now I make it so deselecting all nodes in the graph (like clicking in the background) selects the graph.
	Ref<VoxelGeneratorGraph> graph = _graph_editor->get_graph();
	if (graph.is_valid()) {
		get_editor_interface()->inspect_object(*graph);
	}
}

void VoxelGraphEditorPlugin::_on_graph_editor_nodes_deleted() {
	// When deleting nodes, the selected one can be in them, but the inspector wrapper will still point at it.
	// Clean it up and inspect the graph itself.
	Ref<VoxelGeneratorGraph> graph = _graph_editor->get_graph();
	ERR_FAIL_COND(graph.is_null());
	get_editor_interface()->inspect_object(*graph);
}

template <typename F>
void for_each_node(Node *parent, F action) {
	action(parent);
	for (int i = 0; i < parent->get_child_count(); ++i) {
		for_each_node(parent->get_child(i), action);
	}
}

void VoxelGraphEditorPlugin::_on_graph_editor_regenerate_requested() {
	// We could be editing the graph standalone with no terrain loaded
	if (_voxel_node != nullptr) {
		// Re-generate the selected terrain.
		_voxel_node->restart_stream();

	} else {
		// The node is not selected, but it might be in the tree
		Node *root = get_editor_interface()->get_edited_scene_root();

		if (root != nullptr) {
			Ref<VoxelGeneratorGraph> generator = _graph_editor->get_graph();
			ERR_FAIL_COND(generator.is_null());

			for_each_node(root, [&generator](Node *node) {
				VoxelNode *vnode = Object::cast_to<VoxelNode>(node);
				if (vnode != nullptr && vnode->get_generator() == generator) {
					vnode->restart_stream();
				}
			});
		}
	}
}

void VoxelGraphEditorPlugin::_bind_methods() {
	// ClassDB::bind_method(D_METHOD("_on_graph_editor_node_selected", "node_id"),
	// 		&VoxelGraphEditorPlugin::_on_graph_editor_node_selected);
	// ClassDB::bind_method(
	// 		D_METHOD("_on_graph_editor_nothing_selected"), &VoxelGraphEditorPlugin::_on_graph_editor_nothing_selected);
	ClassDB::bind_method(D_METHOD("_hide_deferred"), &VoxelGraphEditorPlugin::_hide_deferred);
}

} // namespace zylann::voxel
