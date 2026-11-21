#pragma once
#include <filesystem>
#include <vector>
#include "AdminPanel.hpp"
#include <Geode/utils/file.hpp>
#include <Geode/binding/CCMenuItemToggler.hpp>
namespace geobr {
class AdminForm final:public geode::Popup {
    struct Field {std::string key,label,type="text",initial;std::vector<std::string> options;};
    struct Input {Field field;geode::TextInput* text=nullptr;CCMenuItemToggler* toggle=nullptr;cocos2d::CCLabelBMFont* label=nullptr;int choice=0;};
    AdminEntity m_entity;matjson::Value m_row;int64_t m_id=0;std::string m_recordID;std::vector<Input> m_inputs;
    CloudRequest m_save,m_upload;geode::async::TaskHolder<geode::utils::file::PickResult> m_picker;
    cocos2d::CCLabelBMFont* m_status=nullptr;bool m_busy=false;
    bool init(AdminEntity,matjson::Value);std::vector<Field> fields()const;
    void status(std::string const&);void setValue(std::string const&,std::string const&);
    std::string value(std::string const&)const;
    void onSave(cocos2d::CCObject*);void onFile(cocos2d::CCObject*);void onMine(cocos2d::CCObject*);void onFolder(cocos2d::CCObject*);
    void useFile(std::filesystem::path const& path);void onImageFile(cocos2d::CCObject*);void onImageFolder(cocos2d::CCObject*);void onImageClear(cocos2d::CCObject*);void useImage(std::filesystem::path const& path);void uploadGmd(std::vector<uint8_t> bytes,std::string extension,std::string levelName);void onChoice(cocos2d::CCObject*);
public:static AdminForm* create(AdminEntity,matjson::Value row={});
};
}
