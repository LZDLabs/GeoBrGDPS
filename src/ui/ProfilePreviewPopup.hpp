#pragma once
#include <Geode/ui/Popup.hpp>
namespace geobr {class ProfilePreviewPopup final:public geode::Popup {bool init(std::string const&);public:static ProfilePreviewPopup* create(std::string const& style);};}
