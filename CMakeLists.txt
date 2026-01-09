cmake_minimum_required(VERSION 3.20)
project(My2DEngine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(SDL2 CONFIG REQUIRED)

find_package(SDL2_image CONFIG REQUIRED)

find_package(SDL2_ttf CONFIG REQUIRED)

find_package(imgui CONFIG QUIET)

add_executable(game_engine
    src/main.cpp
    src/Engine/Engine.cpp
    src/Renderer/SDLRenderer.cpp
    src/Input/Input.cpp
    src/Time/Time.cpp
    src/Assets/AssetManager.cpp
    src/Assets/AssetManifest.cpp
    src/Game/SandboxScenes.cpp
    src/World/Scene.cpp
    src/World/SceneManager.h
    src/World/Tilemap.cpp
    src/Systems/RenderSystem.cpp
    src/Systems/TilemapSystem.cpp
    src/Systems/PhysicsSystem.cpp
    src/Renderer/CommandBuffer.cpp
)

target_link_libraries(game_engine PRIVATE
    SDL2::SDL2 SDL2::SDL2main
    SDL2_image::SDL2_image
    SDL2_ttf::SDL2_ttf
)

if (imgui_FOUND)
    add_executable(engine_editor
        src/editor_main.cpp
        src/ThirdParty/imgui/backends/imgui_impl_sdl2.cpp
        src/ThirdParty/imgui/backends/imgui_impl_sdlrenderer2.cpp
        src/Engine/Engine.cpp
        src/Renderer/SDLRenderer.cpp
        src/Input/Input.cpp
        src/Time/Time.cpp
        src/Assets/AssetManager.cpp
        src/Assets/AssetManifest.cpp
        src/Game/SandboxScenes.cpp
        src/World/Scene.cpp
        src/World/Tilemap.cpp
        src/Systems/RenderSystem.cpp
        src/Systems/TilemapSystem.cpp
        src/Systems/PhysicsSystem.cpp
        src/Renderer/CommandBuffer.cpp
    )

    target_link_libraries(engine_editor PRIVATE
        SDL2::SDL2 SDL2::SDL2main
        SDL2_image::SDL2_image
        SDL2_ttf::SDL2_ttf
        imgui::imgui
    )

    target_include_directories(engine_editor PRIVATE
        src
    )
else()
    message(WARNING "imgui not found; engine_editor target disabled.")
endif()

