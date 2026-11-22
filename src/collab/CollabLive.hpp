#pragma once
// Live collab editing. When a collab room's level is open in the editor, every
// object carries a room-wide id; about once per second the editor sends its
// added / changed / deleted objects, receives everyone else's and applies them
// in place, and shows each member's name where they are looking in the level.
#include <cstddef>
#include <string>

class EditorUI;
class LevelEditorLayer;

namespace geobr::collab {

/** Starts the live session for this editor (child node of the EditorUI). */
void startLive(EditorUI* ui, std::string const& room);

/** Sends the trace of a live session that ended in a crash (once, at start). */
void uploadPendingTrace();

/** Whether this account may save / publish the room's level (owner or ADM). */
bool canSaveRoom(std::string const& room);

/** This account in the room, from the live session: 3 host, 2 ADM, 1 editor, 0 out / unknown. */
int roomRank(std::string const& room);

/** This editor reloads the room's objects from the server. */
void reloadRoom(EditorUI* ui);

/** Yellow teleport portals without their blue portal (copies of the old collab loop). */
size_t countLooseYellowPortals(LevelEditorLayer* editor);
size_t deleteLooseYellowPortals(LevelEditorLayer* editor);

/** Any editor: a button to delete those yellow portals while there are any. */
void startYellowWatch(EditorUI* ui);

} // namespace geobr::collab
