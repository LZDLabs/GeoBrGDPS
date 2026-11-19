#!/usr/bin/env python3
"""Every GD function the Arrow hooks or calls must be exported by GD 2.2081
for Android (Geode resolves Android hooks by symbol name).
Usage: android_symbols.py <llvm-nm> <libcocos2dcpp.so from the Geode SDK>"""
import subprocess,sys
nm,lib=sys.argv[1],sys.argv[2]
out=subprocess.run([nm,'-DC','--defined-only',lib],capture_output=True,text=True).stdout
needed=['PlayerObject::pushButton(','PlayerObject::releaseButton(','PlayerObject::updateJump(','PlayerObject::update(',
 'PlayerObject::toggleDartMode(','PlayerObject::resetObject(','PlayerObject::playerDestroyed(',
 'GJBaseGameLayer::playerWillSwitchMode(','GJBaseGameLayer::toggleDualMode(','GJBaseGameLayer::update(',
 'GJBaseGameLayer::setupLevelStart(','PlayLayer::createCheckpoint(','PlayLayer::loadFromCheckpoint(',
 'PlayLayer::resetLevel(','PlayLayer::setupHasCompleted(','LevelEditorLayer::onPlaytest(','LevelEditorLayer::init(',
 'LevelSettingsLayer::init(','LevelSettingsLayer::selectSettingClosed(','LevelSettingsObject::getSaveString(',
 'LevelSettingsObject::objectFromDict(','EditorUI::setupCreateMenu(','EditorUI::onCreateButton(','EditorUI::createObject(',
 'EditorUI::getCreateBtn(','EditButtonBar::loadFromItems(','ProfilePage::loadPageFromUserInfo(',
 # RC7: icon kit tab and God Editor
 'GJGarageLayer::init(','GJGarageLayer::onSelectTab(','PauseLayer::customSetup(','PauseLayer::onResume(',
 'GJBaseGameLayer::checkCollisions(','PlayLayer::destroyPlayer(','PlayLayer::levelComplete(','PlayLayer::addObject(',
 'GJBaseGameLayer::updateColor(','cocos2d::ZipUtils::compressString(','cocos2d::ZipUtils::decompressString(',
 'GManager::save(','GameObject::createWithKey(','GameObject::getSaveString(','ObjectToolbox::intKeyToFrame(','ObjectToolbox::sharedState(',
 # RC8: God Editor end-animation guards
 'PlayLayer::playEndAnimationToPos(','PlayLayer::playPlatformerEndAnimationToPos(',
 # 1.14.4: collab deletes like the player (select + delete)
 'EditorUI::selectObjects(','EditorUI::onDeleteSelected(',
 # 1.12.0: live collab editor, RobTop-style rate popup, publish lock
 'LevelEditorLayer::createObjectsFromString(','LevelEditorLayer::removeObject(','EditorUI::deselectAll(',
 'GJDifficultySprite::create(','GJDifficultySprite::updateFeatureState(','EditLevelLayer::onShare(','EditorPauseLayer::saveLevel(',
 # RC9: live God Editor objects
 'GJBaseGameLayer::sortSectionVector(','GJBaseGameLayer::addToSection(','GameObject::activateObject(',
 # RC9: collab editor, credits and full player icons
 'LevelBrowserLayer::init(','EditorUI::init(','EditorUI::deselectAll(','EditorPauseLayer::saveLevel(','UploadPopup::levelUploadFinished(',
 'LevelCell::loadFromLevel(','LevelInfoLayer::init(','LevelEditorLayer::getLevelString(','LevelEditorLayer::createObjectsFromString(',
 'LevelEditorLayer::removeObject(','GameLevelManager::createNewLevel(','EditLevelLayer::scene(','LocalLevelManager::sharedState(',
 'SimplePlayer::enableCustomGlowColor(','SimplePlayer::updateColors(',
 # GDPS: request redirect, account screens, music library
 'cocos2d::extension::CCHttpClient::send(','AccountRegisterLayer::init(','AccountLoginLayer::loginAccountFinished(',
 'GJAccountManager::unlinkFromAccount(','GJAccountManager::sharedState(','MusicDownloadManager::sharedState(',
 'MusicDownloadManager::getSongInfo(','MusicDownloadManager::isSongDownloaded(','AccountLayer::create(','LevelBrowserLayer::scene(',
 'GJSearchObject::create(',
 # GDPS v2: separate save folder (companion), server switch, in-game moderation
 'cocos2d::CCFileUtilsAndroid::getWritablePath(','MenuLayer::init(','ProfilePage::loadPageFromUserInfo(','GManager::save(']
missing=[s for s in needed if s not in out]
assert not missing,f'missing symbols: {missing}'
print(f'PASS: {len(needed)} hooked/called GD functions (Arrow, icon kit, God Editor, collab, player icons, GDPS) exported by libcocos2dcpp.so (Android 2.2081)')
