#include "PlayerService.hpp"
#include <Geode/binding/GJAccountManager.hpp>
#include <Geode/binding/GameManager.hpp>
namespace geobr {PlayerIdentity PlayerService::current(){PlayerIdentity x;if(auto a=GJAccountManager::sharedState()){x.username=a->m_username;x.accountId=a->m_accountID;}if(auto g=GameManager::sharedState()){x.playerId=g->m_playerUserID.value();if(x.username.empty())x.username=g->m_playerName;}return x;}}
