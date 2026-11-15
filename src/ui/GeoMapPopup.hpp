#pragma once
#include <Geode/ui/Popup.hpp>
#include <Geode/ui/ScrollLayer.hpp>
#include "../services/MainLevelsService.hpp"
#include "../services/MainLevelLauncher.hpp"
namespace geobr {
class GeoMapPopup final : public geode::Popup {
 bool init() override;
public:static GeoMapPopup* create();
};
}
