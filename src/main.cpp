
#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <iostream>

int main()
{
    // Initialize SDL
    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << '\n';
        return 1;
    }

    // Create window
    SDL_Window* window = SDL_CreateWindow(
        "Desktop Companion - Cat Test",
        800,
        600,
        SDL_WINDOW_RESIZABLE
    );

    if (window == nullptr)
    {
        std::cerr << "Window creation failed: " << SDL_GetError() << '\n';
        SDL_Quit();
        return 1;
    }

    // Create renderer
    SDL_Renderer* renderer = SDL_CreateRenderer(window, nullptr);

    if (renderer == nullptr)
    {
        std::cerr << "Renderer creation failed: " << SDL_GetError() << '\n';
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    // Load our PNG as a texture
    SDL_Texture* cat = IMG_LoadTexture(renderer, "assets/test.png");

    if (cat == nullptr)
    {
        std::cerr << "Image loading failed: " << SDL_GetError() << '\n';
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    // Get the image dimensions
    float catWidth, catHeight;
    SDL_GetTextureSize(cat, &catWidth, &catHeight);

    bool running = true;

    // Main loop
    while (running)
    {
        SDL_Event event;

        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_EVENT_QUIT)
                running = false;
        }

        // Clear the background
        SDL_SetRenderDrawColor(renderer, 30, 30, 40, 255);
        SDL_RenderClear(renderer);

        // Get the current renderer output size
        int outputWidth = 0;
        int outputHeight = 0;
        
        if (!SDL_GetRenderOutputSize(renderer, &outputWidth, &outputHeight))
        {
            std::cerr << "Failed to get render output size: "
                      << SDL_GetError() << '\n';
            running = false;
            break;
        }

        // Center the cat dynamically
        SDL_FRect destination = {
            (outputWidth - catWidth) / 2.0f,
            (outputHeight - catHeight) / 2.0f,
            catWidth,
            catHeight
        };

        SDL_RenderTexture(renderer, cat, nullptr, &destination);

        SDL_RenderPresent(renderer);
    }

    // Clean up
    SDL_DestroyTexture(cat);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
