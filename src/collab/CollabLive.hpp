#pragma once
// Live collab editing. When a collab room's level is open in the editor, every
// object carries a room-wide id; about once per second the editor sends its
// added / changed / deleted objects, receives everyone else's and applies them
// in place, and shows each member's name where they are looking in the level.
#include <string>

class EditorUI;

namespace geobr::collab {

/** Starts the live session for this editor (child node of the EditorUI). */
void startLive(EditorUI* ui, std::string const& room);

/** Sends the trace of a live session that ended in a crash (once, at start). */
void uploadPendingTrace();

/** Whether this account may save / publish the room's level (owner or ADM). */
bool canSaveRoom(std::string const& room);

} // namespace geobr::collab
