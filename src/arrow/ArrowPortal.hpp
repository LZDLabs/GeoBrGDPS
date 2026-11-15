#pragma once

#include <Geode/Geode.hpp>

class GameObject;
class GJBaseGameLayer;

namespace geobr::arrow {

// The Arrow portal is stored as a native wave portal (ID 660) marked with the
// native "Hide" flag and a reserved Z order. Both are plain GD properties, so
// the marker survives save, copy/paste, undo and upload without custom keys.
// Without GeoBR the portal still behaves as an (invisible) wave portal.
namespace portal {
inline constexpr int BaseObjectID = 660;
inline constexpr int MarkerZOrder = 97;

bool isArrowPortal(GameObject* object);
bool isGamemodePortal(GameObject* object);
void markAsArrowPortal(GameObject* object);

// Draws the brown portal on top of every Arrow portal of the layer and keeps
// it in sync with the object (position, rotation, scale, selection).
void attachOverlays(GJBaseGameLayer* layer, bool rescanPeriodically);
void track(GJBaseGameLayer* layer, GameObject* object);

// Portal art used for buttons and previews.
cocos2d::CCNode* createPreview(float height);
} // namespace portal

} // namespace geobr::arrow
