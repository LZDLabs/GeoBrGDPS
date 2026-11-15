#include "MainLevelLauncher.hpp"

#include "ApiClient.hpp"
#include "../config/GeoBRConfig.hpp"
#include "MainLevelProgress.hpp"

#include <Geode/binding/GJGameLevel.hpp>
#include <Geode/utils/file.hpp>
#include <hjfod.gmd-api/include/GMD.hpp>

#include <filesystem>
#include <fmt/format.h>
#include <system_error>

using namespace geode::prelude;

namespace geobr {
namespace {

std::filesystem::path cachePath(MainLevel const& level) {
    auto extension = std::filesystem::path(level.gmdPath).extension().string();
    if (extension != ".gmd" && extension != ".gmd2" && extension != ".lvl") extension = ".gmd";
    return Mod::get()->getSaveDir() / "main-level-cache" /
        fmt::format("{}-v{}{}", level.id, level.fileVersion, extension);
}

ImportedMainLevel fail(std::string message) {
    return {.error = std::move(message)};
}

void applySupabaseMetadata(GJGameLevel* level, MainLevel const& metadata) {
    level->setLevelID(MainLevelProgress::localLevelID(metadata.id));
    level->m_levelName = metadata.name;
    level->m_creatorName = metadata.creator;
    level->m_stars = metadata.stars;
    level->m_levelType = GJLevelType::Saved;
    level->m_isEditable = false;
    level->m_localOrSaved = true;
    level->m_levelNotDownloaded = false;
    level->m_dontSave = false;
    level->m_isVerifiedRaw = true;
    level->m_isVerified = true;

    auto difficulty = metadata.difficultyCode();
    level->m_autoLevel = difficulty == 0;
    level->m_demon = difficulty >= 6;
    if (difficulty >= 6) {
        level->m_difficulty = GJDifficulty::Demon;
        level->m_demonDifficulty = difficulty;
    }
    else if (difficulty >= 0) {
        level->m_difficulty = static_cast<GJDifficulty>(difficulty);
    }
}

}

geode::utils::web::WebFuture MainLevelLauncher::download(MainLevel const& level) {
    return ApiClient::getBinary(GeoBRConfig::supabaseStorageUrl(level.gmdPath));
}

ImportedMainLevel MainLevelLauncher::cached(MainLevel const& metadata){
 if(!GeoBRConfig::validStoragePath(metadata.gmdPath))return fail("Arquivo invalido");
 auto path=cachePath(metadata);std::error_code ec;if(!std::filesystem::is_regular_file(path,ec))return fail("Level ainda nao baixado");
 auto result=gmd::importGmdAsLevel(path);if(result.isErr()||!result.unwrap()||result.unwrap()->m_levelString.empty())return fail("Cache invalido");
 auto level=result.unwrap();applySupabaseMetadata(level,metadata);return {.level=level};
}

ImportedMainLevel MainLevelLauncher::import(
    MainLevel const& metadata,
    geode::utils::web::WebResponse const& response
) {
    if (!GeoBRConfig::validStoragePath(metadata.gmdPath)) {
        return fail("O caminho do arquivo .gmd publicado no Supabase e invalido.");
    }
    if (!response.ok()) {
        if(auto local=cached(metadata);local)return local;
        auto detail = std::string(response.errorMessage());
        if (detail.empty()) detail = "arquivo indisponivel";
        if (detail.size() > 100) detail.resize(100);
        return fail(fmt::format("Nao foi possivel baixar o level (HTTP {}): {}", response.code(), detail));
    }
    auto const& bytes = response.data();
    if (bytes.empty()) return fail("O arquivo .gmd esta vazio.");
    if (bytes.size() > GeoBRConfig::MaxGmdBytes) {
        return fail("O arquivo .gmd ultrapassa o limite de 12 MB.");
    }

    auto path = cachePath(metadata);
    std::error_code directoryError;
    std::filesystem::create_directories(path.parent_path(), directoryError);
    if (directoryError) return fail("Nao foi possivel preparar o cache do level.");
    if (auto written = geode::utils::file::writeBinary(path, bytes); written.isErr()) {
        return fail(fmt::format("Nao foi possivel salvar o .gmd: {}", written.unwrapErr()));
    }

    auto imported = gmd::importGmdAsLevel(path);
    if (imported.isErr()) {
        return fail(fmt::format("O GDShare recusou o arquivo .gmd: {}", imported.unwrapErr()));
    }
    auto level = imported.unwrap();
    if (!level || level->m_levelString.empty()) return fail("O .gmd nao contem dados de level validos.");

    applySupabaseMetadata(level, metadata);
    return {.level = level};
}

}
