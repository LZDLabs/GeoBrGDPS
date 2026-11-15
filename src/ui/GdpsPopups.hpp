#pragma once
#include "../gdps/GdpsService.hpp"
#include <Geode/ui/Popup.hpp>
#include <Geode/ui/ScrollLayer.hpp>
#include <Geode/ui/TextInput.hpp>
#include <Geode/utils/file.hpp>
#include <Geode/binding/CCMenuItemSpriteExtra.hpp>
#include <Geode/binding/CCMenuItemToggler.hpp>
#include <functional>
#include <string>
#include <vector>

// GeoBR GDPS screens: account status, recovery, Owner claim, Music Library,
// and the admin panel (GdpsAdminPopup.cpp).
namespace geobr {

// Small form: a few text fields and a row of buttons.
class GdpsPrompt final : public geode::Popup {
public:
    struct Field {
        std::string label;
        std::string value;
        geode::CommonFilter filter = geode::CommonFilter::Any;
        int max = 120;
        bool password = false;
    };
    using Done = std::function<void(int button, std::vector<std::string> const& values)>;
    static GdpsPrompt* create(std::string const& title, std::string const& text, std::vector<Field> fields,
                              std::vector<std::string> buttons, Done done);
private:
    std::vector<geode::TextInput*> m_inputs;
    Done m_done;
    bool init(std::string const& title, std::string const& text, std::vector<Field> fields, std::vector<std::string> buttons, Done done);
    void onButton(cocos2d::CCObject*);
};

// A scrolling list of text rows, each with optional buttons (shared by the GDPS screens).
struct GdpsRow {
    std::string text;
    cocos2d::ccColor3B color {255, 255, 255};
    std::vector<std::string> buttons;
};

class GdpsList final : public cocos2d::CCNode {
public:
    using Pressed = std::function<void(int row, int button)>;
    static GdpsList* create(cocos2d::CCSize size, Pressed pressed);
    void setRows(std::vector<GdpsRow> rows);
private:
    geode::ScrollLayer* m_scroll = nullptr;
    Pressed m_pressed;
    cocos2d::CCSize m_size;
    bool init(cocos2d::CCSize size, Pressed pressed);
    void onPress(cocos2d::CCObject*);
};

class GdpsPopup final : public geode::Popup {
public:
    static GdpsPopup* create();
private:
    gdps::Request m_me, m_action;
    matjson::Value m_info = matjson::Value::object();
    cocos2d::CCNode* m_content = nullptr;
    cocos2d::CCLabelBMFont* m_status = nullptr;
    bool init();
    void refresh();
    void render();
    void status(std::string const&);
    void onMusic(cocos2d::CCObject*);
    void onRecoveryCode(cocos2d::CCObject*);
    void onResetPassword(cocos2d::CCObject*);
    void onAdmin(cocos2d::CCObject*);
    void onOwner(cocos2d::CCObject*);
    void onAccount(cocos2d::CCObject*);
    void onSwitch(cocos2d::CCObject*);
    void onPassword(cocos2d::CCObject*);
};

// Oficial <-> GDPS GeoBR (each with its own save and account; restarts the game).
class ServerSwitchPopup final : public geode::Popup {
public:
    static ServerSwitchPopup* create();
private:
    bool init();
    void onPick(cocos2d::CCObject*);
};

// In-game moderation of one level (button on the level page, staff only).
class LevelModPopup final : public geode::Popup {
public:
    static LevelModPopup* create(int levelID, std::string name);
private:
    gdps::Request m_action;
    int m_level = 0;
    std::string m_name;
    cocos2d::CCLabelBMFont* m_status = nullptr;
    bool init(int levelID, std::string name);
    void status(std::string const&);
    void act(std::string const& action, matjson::Value fields, std::string const& done);
    void onAction(cocos2d::CCObject*);
};

// In-game moderation of one player (button on the profile, staff only).
class UserModPopup final : public geode::Popup {
public:
    static UserModPopup* create(int accountID, std::string name);
private:
    gdps::Request m_fetch, m_action;
    int m_account = 0;
    std::string m_name;
    GdpsList* m_list = nullptr;
    cocos2d::CCLabelBMFont* m_status = nullptr;
    cocos2d::CCLabelBMFont* m_state = nullptr;
    bool init(int accountID, std::string name);
    void refresh();
    void status(std::string const&);
    void act(std::string const& action, matjson::Value fields, std::string const& done);
    void onAction(cocos2d::CCObject*);
};

class MusicLibraryPopup final : public geode::Popup {
public:
    static MusicLibraryPopup* create();
private:
    gdps::Request m_fetch, m_upload;
    geode::async::TaskHolder<geode::utils::file::PickResult> m_picker;
    geode::async::TaskHolder<geode::utils::web::WebResponse> m_put;
    geode::TextInput* m_search = nullptr;
    GdpsList* m_list = nullptr;
    cocos2d::CCLabelBMFont* m_status = nullptr;
    matjson::Value m_songs = matjson::Value::array();
    bool m_mine = false, m_busy = false;
    bool init();
    void fetch();
    void status(std::string const&);
    void onSearch(cocos2d::CCObject*);
    void onMine(cocos2d::CCObject*);
    void onUpload(cocos2d::CCObject*);
    void pickAndSend(std::string name, std::string artist);
    void useSong(int row);
};

class GdpsAdminPopup final : public geode::Popup {
public:
    static GdpsAdminPopup* create(matjson::Value me);
private:
    enum Tab { Stats, Players, Levels, Comments, Events, Demons, Songs, Roles, Board, Audit, Packs, TabCount };
    matjson::Value m_me;
    gdps::Request m_fetch, m_action;
    Tab m_tab = Stats;
    int m_sub = 0;                       // sub-view inside a tab (e.g. reports / suggestions)
    std::string m_query;
    matjson::Value m_rows = matjson::Value::array();
    std::vector<std::vector<std::string>> m_rowButtons;
    matjson::Value m_roles = matjson::Value::array();
    std::vector<CCMenuItemSpriteExtra*> m_tabButtons;
    cocos2d::CCMenu* m_tools = nullptr;
    geode::TextInput* m_search = nullptr;
    GdpsList* m_list = nullptr;
    cocos2d::CCLabelBMFont* m_status = nullptr;
    bool init(matjson::Value me);
    bool has(char const* permission) const;
    bool tabAllowed(Tab) const;
    void selectTab(Tab);
    void buildTools();
    void load();
    void showRows(matjson::Value rows);
    void status(std::string const&);
    void act(std::string const& action, matjson::Value fields, std::string const& done);
    void onTab(cocos2d::CCObject*);
    void onTool(cocos2d::CCObject*);
    void onRow(int row, int button);
    void editPack(matjson::Value pack);
    void editGauntlet(matjson::Value gauntlet);
};

// Role editor: name, colour, position and the permission switches.
class GdpsRolePopup final : public geode::Popup {
public:
    using Saved = std::function<void()>;
    static GdpsRolePopup* create(matjson::Value role, matjson::Value catalog, Saved saved);
private:
    matjson::Value m_role;
    std::vector<std::string> m_catalog;
    std::vector<CCMenuItemToggler*> m_toggles;
    geode::TextInput *m_name = nullptr, *m_color = nullptr, *m_position = nullptr;
    cocos2d::CCLabelBMFont* m_status = nullptr;
    gdps::Request m_save;
    Saved m_saved;
    bool init(matjson::Value role, matjson::Value catalog, Saved saved);
    void onSave(cocos2d::CCObject*);
};

} // namespace geobr
