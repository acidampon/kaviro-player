#include "ump/HistoryStore.h"
#include "ump/MediaLibrary.h"
#include "ump/MediaProbe.h"
#include "ump/PlaybackEngine.h"
#include "ump/PlaybackController.h"
#include <filesystem>
#include <iostream>
#include <string>

static void usage() {
    std::cout << "KAVIRO Player — standalone core prototype
"
              << "Usage:
"
              << "  kaviro_player <file>                 Inspect and play a local media file
"
              << "  kaviro_player --inspect <file>       Inspect without playback
"
              << "  kaviro_player --scan <folder>        Scan a media folder
"
              << "  kaviro_player --history <database>   Show recent local playback history
"
              << "  kaviro_player --version              Show build version
"
              << "  kaviro_player --engine-info          Show media-engine status
";
}

static void printProbe(const ump::MediaProbeResult& probe) {
    std::cout << "Format: " << probe.format << "
"
              << "Codec: " << probe.codec << "
"
              << "Type: " << (probe.type == ump::MediaType::Video ? "video" : probe.type == ump::MediaType::Audio ? "audio" : "unknown") << "
"
              << "Duration: " << probe.durationMs << " ms
";
    if (probe.bitrate) std::cout << "Bitrate: " << probe.bitrate << " bps
";
    if (probe.width && probe.height) std::cout << "Video: " << probe.width << 'x' << probe.height << '
';
}

int main(int argc, char** argv) {
    if (argc < 2) { usage(); return 0; }
    const std::string arg = argv[1];

    if (arg == "--version") {
        std::cout << "KAVIRO Player 0.6.0 — playback/library foundation
";
        return 0;
    }
    if (arg == "--engine-info") {
        ump::ExternalPlayerAdapter engine;
        const auto info = engine.info();
        std::cout << "Engine: " << info.name << "
"
                  << "Available: " << (info.available ? "yes" : "no") << "
"
                  << "Standalone: " << (info.standalone ? "yes" : "no") << "
"
                  << "Seek: " << (engine.supports(ump::EngineCapability::Seek) ? "yes" : "no") << "
"
                  << "Pause: " << (engine.supports(ump::EngineCapability::Pause) ? "yes" : "no") << "
"
                  << "Track selection: " << (engine.supports(ump::EngineCapability::AudioTracks) ? "yes" : "no") << "
";
        for (const auto& note : info.notes) std::cout << "Note: " << note << "
";
        return info.available ? 0 : 6;
    }
    if (arg == "--inspect") {
        if (argc < 3) { usage(); return 2; }
        const auto probe = ump::MediaProbe::inspect(argv[2]);
        if (!probe.success) { std::cerr << "Cannot inspect media: " << probe.error << '
'; return 3; }
        printProbe(probe); return 0;
    }
    if (arg == "--scan") {
        if (argc < 3) { usage(); return 2; }
        ump::MediaLibrary library;
        const auto items = library.scan(argv[2]);
        std::cout << "Found " << items.size() << " media files.
";
        for (const auto& item : items)
            std::cout << (item.type == ump::MediaType::Video ? "VIDEO " : "AUDIO ") << item.path.string() << " [" << item.durationMs << " ms]
";
        return 0;
    }
    if (arg == "--history") {
        if (argc < 3) { usage(); return 2; }
        ump::HistoryStore history(argv[2]);
        if (!history.open()) { std::cerr << "Cannot open history: " << history.lastError() << '
'; return 5; }
        for (const auto& entry : history.recent())
            std::cout << entry.title << " | " << entry.path.string() << " | position=" << entry.positionMs << " ms | plays=" << entry.playCount << '
';
        return 0;
    }

    const auto probe = ump::MediaProbe::inspect(arg);
    if (!probe.success) { std::cerr << "Cannot inspect media: " << probe.error << '
'; return 3; }
    printProbe(probe);
    ump::PlaybackController controller;
    const auto absolute = std::filesystem::absolute(arg).lexically_normal();
    controller.load(absolute.string(), probe.durationMs);
    controller.play();
    ump::ExternalPlayerAdapter player;
    return player.play(absolute.string()) ? 0 : 4;
}
