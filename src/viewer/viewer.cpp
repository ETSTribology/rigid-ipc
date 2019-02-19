#include "viewer.hpp"

#include <igl/bounding_box.h>
#include <igl/opengl/MeshGL.h>
#include <igl/unproject_onto_mesh.h>

#include <ccd/not_implemented_error.hpp>

#include <io/read_scene.hpp>

namespace ccd {

ViewerMenu::ViewerMenu(std::string scene_file)
    : edit_mode(ViewerEditMode::select)
    , color_vtx(1.0, 0.0, 0.0)
    , color_edge(1.0, 0.0, 0.0)
    , color_displ(0.0, 1.0, 0.0)
    , color_grad(0.0, 0.0, 1.0)
    , color_canvas(0.3, 0.3, 0.5) // #4c4c80
    , color_sl(1.0, 1.0, 0.0)
    , scene_file(scene_file)
    , last_action_message("")
    , last_action_success(true)
{
    // clang-format off
    canvas_nodes = (Eigen::MatrixXd(4, 3) <<
        -.5, -.5, 0.0,
        -.5, 0.5, 0.0,
        0.5, -.5, 0.0,
        0.5, 0.5, 0.0).finished();

    canvas_faces = (Eigen::MatrixXi(2, 3) <<
        0, 2, 1,
        2, 3, 1).finished();
    // clang-format on
}

void ViewerMenu::init(igl::opengl::glfw::Viewer* _viewer)
{
    Super::init(_viewer);

    // STRUCTURE
    {
        surface_data_id = viewer->data_list.size() - 1;
        viewer->append_mesh();
        displ_data_id = viewer->data_list.size() - 1;
        viewer->append_mesh();
        gradient_data_id = viewer->data_list.size() - 1;
        load_scene(scene_file);
    }

    // CANVAS
    {
        viewer->append_mesh();
        viewer->data().set_mesh(canvas_nodes, canvas_faces);
        viewer->data().show_lines = false;
        viewer->data().set_colors(color_canvas);
        canvas_data_id = viewer->data_list.size() - 1;
        resize_canvas();
    }
}

bool ViewerMenu::save_scene()
{
    std::string fname = igl::file_dialog_save();
    if (fname.length() == 0)
        return false;
    state.save_scene(fname);
    return true;
}

bool ViewerMenu::load_scene()
{
    std::string fname = igl::file_dialog_open();
    if (fname.length() == 0)
        return false;
    return load_scene(fname);
}

bool ViewerMenu::load_scene(const std::string filename)
{

    if (filename.empty()) {
        io::read_scene_from_str(
            default_scene, state.vertices, state.edges, state.displacements);
    } else {
        state.load_scene(filename);
    }

    state_history.clear();
    state_history.push_back(state);

    load_state();
    return true;
}

void ViewerMenu::load_state()
{
    viewer->data_list[surface_data_id].set_points(state.vertices, color_vtx);
    viewer_set_edges(surface_data_id, state.vertices, state.edges, color_edge);
    viewer->data_list[surface_data_id].point_size = 10 * pixel_ratio();
    // only needed to show vertex_ids
    viewer->data_list[surface_data_id].set_vertices(state.vertices);
    Eigen::MatrixXd nz(state.vertices.rows(), 3);
    nz.col(2).setConstant(1.0);
    viewer->data_list[surface_data_id].set_normals(nz);

    viewer->data_list[displ_data_id].set_points(
        state.vertices + state.displacements, color_displ);
    viewer->data_list[displ_data_id].set_edges(
        Eigen::MatrixXd(), Eigen::MatrixXi(), color_edge);
    viewer->data_list[displ_data_id].add_edges(
        state.vertices, state.vertices + state.displacements, color_displ);
    viewer->data_list[displ_data_id].point_size = 10 * pixel_ratio();

    viewer->data_list[gradient_data_id].set_points(state.vertices, color_grad);
    viewer->data_list[gradient_data_id].set_edges(
        Eigen::MatrixXd(), Eigen::MatrixXi(), color_grad);
    viewer->data_list[gradient_data_id].add_edges(
        state.vertices, state.vertices, color_grad);
    viewer->data_list[gradient_data_id].point_size = 1;

    vertices_colors.resize(state.vertices.rows(), 3);

    recolor_vertices();
    redraw_at_time();
}

void ViewerMenu::resize_canvas()
{
    Eigen::MatrixXd V = canvas_nodes;
    V.col(0) *= state.canvas_width;
    V.col(1) *= state.canvas_height;
    viewer->data_list[canvas_data_id].set_vertices(V);
}

// ----------------------------------------------------------------------------------------------------------------------------
// USER ACTIONS
// ----------------------------------------------------------------------------------------------------------------------------
void ViewerMenu::undo()
{
    if (state_history.size() > 1) {
        state_history.pop_back();
        state = state_history.back();
        load_state();
    }
}

void ViewerMenu::connect_selected_vertices()
{
    size_t num_sl = state.selected_points.size();
    if (num_sl < 2) {
        return;
    }
    Eigen::MatrixXi new_edges(num_sl - 1, 2);
    for (uint i = 0; i < num_sl - 1; ++i) {
        new_edges.row(i) << state.selected_points[i],
            state.selected_points[i + 1];
    }
    state.add_edges(new_edges);
    add_graph_edges(surface_data_id, state.vertices, new_edges, color_edge);
    state_history.push_back(state);
}

bool update_selection(const Eigen::MatrixXd& points, const int modifier,
    const Eigen::RowVector2d& click_coord, const double thr,
    std::vector<int>& selection)
{
    // If a vertex was clicked while holding _shift_ it is added to the selected
    // points. If _shift_ was not clicked, the selection
    //      is replaced by clicked-vertex.
    int index;
    double dist
        = (points.rowwise() - click_coord).rowwise().norm().minCoeff(&index);
    if (dist < thr) {
        if (modifier == GLFW_MOD_SHIFT) {
            selection.push_back(index);
        } else {
            selection.clear();
            selection.push_back(index);
        }
        return true;
    } else {
        selection.clear();
        return false;
    }
}

void translation_delta(const Eigen::MatrixXd& points, const int modifier,
    const Eigen::RowVector2d& click_coord, const std::vector<int>& selection,
    Eigen::RowVector2d& delta)
{
    size_t num_sl = selection.size();
    assert(num_sl > 0);

    Eigen::RowVector2d current = points.row(selection[num_sl - 1]);
    delta = click_coord - current;

    if (modifier == GLFW_MOD_SHIFT) {
        int index;
        (delta).cwiseAbs().maxCoeff(&index);

        double aux = delta[index];
        delta.setZero();
        delta[index] = aux;
    }
}

void ViewerMenu::clicked__select(
    const int /*button*/, const int modifier, const Eigen::RowVector2d& coord)
{
    double thr = state.canvas_width / 100;
    bool clicked_vertex = update_selection(
        state.vertices, modifier, coord, thr, state.selected_points);
    if (clicked_vertex) {
        state.selected_displacements.clear();
    } else {
        update_selection(state.vertices + state.displacements, modifier, coord,
            thr, state.selected_displacements);
    }

    recolor_vertices();
}

void ViewerMenu::clicked__translate(
    const int /*button*/, const int modifier, const Eigen::RowVector2d& coord)
{
    size_t num_sl = state.selected_points.size();
    if (num_sl > 0) {
        Eigen::RowVector2d delta;
        translation_delta(
            state.vertices, modifier, coord, state.selected_points, delta);
        for (size_t i = 0; i < num_sl; ++i) {
            state.move_vertex(state.selected_points[i], delta);
        }
        redraw_vertices();
    } else {
        size_t num_sl = state.selected_displacements.size();
        if (num_sl > 0) {
            Eigen::RowVector2d delta;
            translation_delta(state.vertices + state.displacements, modifier,
                coord, state.selected_displacements, delta);
            for (size_t i = 0; i < num_sl; ++i) {
                state.move_displacement(state.selected_displacements[i], delta);
            }
            redraw_displacements();
        }
    }
}

void ViewerMenu::clicked__add_node(const int /*button*/, const int /*modifier*/,
    const Eigen::RowVector2d& coord)
{
    state.add_vertex(coord);
    add_graph_vertex(surface_data_id, state.vertices, coord, color_vtx);
    extend_vector_field(
        displ_data_id, state.vertices, state.displacements, 1, color_displ);
    extend_vector_field(
        gradient_data_id, state.vertices, state.volume_grad, 1, color_grad);
}

void ViewerMenu::clicked__add_chain(const int /*button*/,
    const int /*modifier*/, const Eigen::RowVector2d& /*coord*/)
{
    last_action_message = "Adding a chain is not implemented yet.";
    last_action_success = false;
}

void ViewerMenu::clicked_on_canvas(
    const int button, const int modifier, const Eigen::RowVector2d& coord)
{
    switch (edit_mode) {
    case select:
        clicked__select(button, modifier, coord);
        break;
    case translate:
        clicked__translate(button, modifier, coord);
        break;
    case add_chain:
        clicked__add_chain(button, modifier, coord);
        break;
    case add_node:
        clicked__add_node(button, modifier, coord);
        break;
    }
    if (edit_mode == translate || edit_mode == add_node) {
        state_history.push_back(state);
    }
}

bool ViewerMenu::mouse_down(int button, int modifier)
{
    if (Super::mouse_down(button, modifier)) {
        return true;
    }

    // pick a node
    double x = viewer->current_mouse_x;
    double y = double(viewer->core.viewport(3)) - viewer->current_mouse_y;

    int face_id;
    Eigen::Vector3d barycentric_coords;

    Eigen::MatrixXd BV = viewer->data_list[canvas_data_id].V;
    if (igl::unproject_onto_mesh(Eigen::Vector2f(x, y), viewer->core.view,
            viewer->core.proj, viewer->core.viewport, BV, canvas_faces, face_id,
            barycentric_coords)) {
        Eigen::Vector3i face_nodes = canvas_faces.row(face_id);
        Eigen::RowVector3d coords
            = BV.row(face_nodes[0]) * barycentric_coords[0]
            + BV.row(face_nodes[1]) * barycentric_coords[1]
            + BV.row(face_nodes[2]) * barycentric_coords[2];

        // pass 2d coords
        clicked_on_canvas(button, modifier, coords.segment(0, 2));
    }
    return true;
}
bool ViewerMenu::key_pressed(unsigned int key, int modifiers)
{
    if (Super::key_pressed(key, modifiers)) {
        return true;
    }

    if (key == 'q') {
        edit_mode = ViewerEditMode::select;
    } else if (key == 'w') {
        edit_mode = ViewerEditMode::translate;
    } else if (key == 'Z' && modifiers == GLFW_MOD_SHIFT) {
        undo();
    } else {
        return false;
    }
    return true;
}

// ----------------------------------------------------------------------------------------------------------------------------
// CCD USER ACTIONS
// ----------------------------------------------------------------------------------------------------------------------------
void ViewerMenu::detect_edge_vertex_collisions()
{
    try {
        state.detect_edge_vertex_collisions();
    } catch (NotImplementedError e) {
        last_action_message = e.what();
        last_action_success = false;
    }
}

void ViewerMenu::compute_collision_volumes()
{
    try {
        state.compute_collision_volumes();
    } catch (NotImplementedError e) {
        last_action_message = e.what();
        last_action_success = false;
    }
}

void ViewerMenu::goto_impact(const int impact)
{
    if (impact == -1) {
        state.time = 0.0;
        state.current_impact = impact;
        redraw_at_time();
    } else if (state.impacts != nullptr && state.impacts->size() > 0) {
        state.current_impact = impact;
        state.current_impact %= state.impacts->size();

        state.time
            = float(state.impacts->at(size_t(state.current_impact))->time);
        redraw_at_time();
    }
}

// ----------------------------------------------------------------------------------------------------------------------------
// DRAWING
// ----------------------------------------------------------------------------------------------------------------------------
void ViewerMenu::recolor_vertices()
{
    color_points(surface_data_id, color_vtx);
    highlight_points(surface_data_id, state.selected_points, color_sl);

    color_points(displ_data_id, color_displ);
    highlight_points(displ_data_id, state.selected_displacements, color_sl);
}

void ViewerMenu::redraw_vertices()
{
    // surface
    update_graph(surface_data_id, state.vertices, state.edges);

    // displacements
    update_vector_field(displ_data_id, state.vertices, state.displacements);

    // volume_gradient: reset to zero
    update_vector_field(gradient_data_id, state.vertices, Eigen::MatrixXd());
}

void ViewerMenu::redraw_displacements()
{
    update_vector_field(displ_data_id, state.vertices, state.displacements);
}

void ViewerMenu::redraw_at_time()
{
    update_graph(surface_data_id,
        state.vertices + state.displacements * double(state.time), state.edges);
}

void ViewerMenu::update_vector_field(const unsigned long data_id,
    const Eigen::MatrixXd& x0, const Eigen::MatrixXd& delta)
{
    Eigen::MatrixXd& lines = viewer->data_list[data_id].lines;
    Eigen::MatrixXd& points = viewer->data_list[data_id].points;
    Eigen::MatrixXd x1;
    if (delta.size() == 0) {
        x1 = x0;
    } else {
        x1 = x0 + delta;
    }

    long dim = x0.cols();
    for (unsigned i = 0; i < lines.rows(); ++i) {
        lines.row(i).segment(0, dim) << x0.row(i);
        lines.row(i).segment(3, dim) << x1.row(i);
    }
    for (unsigned i = 0; i < points.rows(); ++i) {
        points.row(i).segment(0, dim) << x1.row(i);
    }

    viewer->data_list[data_id].dirty
        |= igl::opengl::MeshGL::DIRTY_OVERLAY_LINES;
    viewer->data_list[data_id].dirty
        |= igl::opengl::MeshGL::DIRTY_OVERLAY_POINTS;
}

void ViewerMenu::extend_vector_field(const unsigned long data_id,
    const Eigen::MatrixXd& x0, const Eigen::MatrixXd& delta, const int count,
    const Eigen::RowVector3d& color)
{
    Eigen::MatrixXd x1 = x0 + delta;
    for (int i = 0; i < count; ++i) {
        int j = int(x0.rows()) - 1 - i;
        viewer->data_list[data_id].add_points(x1.row(j), color);
        viewer->data_list[data_id].add_edges(x0.row(j), x1.row(j), color);
    }
}

void ViewerMenu::update_graph(const unsigned long data_id,
    const Eigen::MatrixXd& vertices, const Eigen::MatrixXi& edges)
{
    long dim = vertices.cols();

    viewer->data_list[data_id].set_vertices(vertices);
    Eigen::MatrixXd& points = viewer->data_list[data_id].points;
    for (unsigned i = 0; i < points.rows(); ++i) {
        points.row(i).segment(0, dim) = vertices.row(i);
    }
    Eigen::MatrixXd& lines = viewer->data_list[data_id].lines;
    for (unsigned i = 0; i < edges.rows(); ++i) {
        lines.row(i).segment(0, dim) << vertices.row(edges(i, 0));
        lines.row(i).segment(3, dim) << vertices.row(edges(i, 1));
    }

    viewer->data_list[data_id].dirty
        |= igl::opengl::MeshGL::DIRTY_OVERLAY_POINTS;
    viewer->data_list[data_id].dirty
        |= igl::opengl::MeshGL::DIRTY_OVERLAY_LINES;
}

void ViewerMenu::add_graph_edges(const unsigned long data_id,
    const Eigen::MatrixXd& vertices, const Eigen::MatrixXi& new_edges,
    const Eigen::RowVector3d& color)
{
    long dim = vertices.cols();

    long num_edges = new_edges.rows();
    Eigen::MatrixXd P1(num_edges, dim), P2(num_edges, dim);
    for (uint i = 0; i < num_edges; ++i) {
        P1.row(i) << vertices.row(new_edges(i, 0));
        P2.row(i) << vertices.row(new_edges(i, 1));
    }
    viewer->data_list[data_id].add_edges(P1, P2, color);
}

void ViewerMenu::add_graph_vertex(const unsigned long data_id,
    const Eigen::MatrixXd& vertices, const Eigen::RowVector2d& vertex,
    const Eigen::RowVector3d& color)
{
    viewer->data_list[data_id].set_vertices(vertices);
    Eigen::MatrixXd nz(state.vertices.rows(), 3);
    nz.col(2).setConstant(1.0);
    viewer->data_list[surface_data_id].set_normals(nz);

    viewer->data_list[data_id].add_points(vertex, color);
}

void ViewerMenu::color_points(
    const unsigned long data_id, const Eigen::RowVector3d& color)
{
    Eigen::MatrixXd& points = viewer->data_list[data_id].points;
    points.block(0, 3, points.rows(), 3).rowwise() = color;
    viewer->data_list[data_id].dirty
        |= igl::opengl::MeshGL::DIRTY_OVERLAY_POINTS;
}

void ViewerMenu::highlight_points(const unsigned long data_id,
    const std::vector<int>& nodes, const Eigen::RowVector3d& color_hl)
{
    Eigen::MatrixXd& points = viewer->data_list[data_id].points;
    for (unsigned i = 0; i < nodes.size(); ++i) {
        points.row(nodes[i]).segment(3, 3) = color_hl;
    }

    viewer->data_list[data_id].dirty
        |= igl::opengl::MeshGL::DIRTY_OVERLAY_POINTS;
}

// FIX of LIBIGL VIEWER FUNCTIONS
void ViewerMenu::viewer_set_edges(const unsigned long data_id,
    const Eigen::MatrixXd& P, const Eigen::MatrixXi& E,
    const Eigen::MatrixXd& C)
{
    Eigen::MatrixXd P_temp;

    // If P only has two columns, pad with a column of zeros
    if (P.cols() == 2) {
        P_temp = Eigen::MatrixXd::Zero(P.rows(), 3);
        P_temp.block(0, 0, P.rows(), 2) = P;
    } else {
        P_temp = P;
    }
    viewer->data_list[data_id].set_edges(P_temp, E, C);
}
}