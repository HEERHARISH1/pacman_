#include <SFML/Graphics.hpp>
#include <iostream>
#include <array>
#include <string>
#include <vector>
#include <atomic>
#include <random>
#include <mutex>
#include <pthread.h>
#include <condition_variable>
#include <chrono>
constexpr int LINE_THICKNESS = 2;
constexpr int MAP_WIDTH = 41;
constexpr int MAP_HEIGHT = 22;
constexpr int TILE_SIZE = 30;
constexpr int MAZE_WIDTH = 21;
constexpr int POWER_PELLET_DURATION =1000; // Duration for power pellet effect in milliseconds
constexpr int GHOST_PELLET_SPAWN_TIME = 20;  // Time in seconds after which the ghost pellet spawns
constexpr int GHOST_PELLET_SPEED_MULTIPLIER = 2; // Speed multiplier for ghosts after eating the ghost pellet
int totalPellets = 152;  // Initialize with the total number of pellets
int score = 0;
int lives = 3; // Initialize lives with 3
int pacmanX = 9, pacmanY = 16;
sf::Text scoreText;
sf::Text livesText;
sf::Keyboard::Key lastDirection = sf::Keyboard::Right; // Store last direction of movement
bool powerPelletActive = false; // Flag for power pellet effect
std::chrono::time_point<std::chrono::steady_clock> powerPelletEndTime; // End time for power pellet effect
bool ghostPelletAvailable = false; // Flag for ghost pellet availability
int ghostPelletX = 9, ghostPelletY = 8; // Position for ghost pellet (center above ghost house)
int ghostsEatenGhostPellet = 0; // Number of ghosts that have eaten the ghost pellet

// Define symbols for game board elements
enum class CellType { Wall, Path, Pellet, PowerPellet, Pacman, Ghost, GhostPellet };

struct Cell {
    std::atomic<CellType> type;
    std::atomic<char> character; // Character representation on the map
};

std::array<std::array<Cell, MAP_WIDTH>, MAP_HEIGHT> gameBoard;

struct Ghost {
    int x, y;
    int dx = 0, dy = 0; // Store direction as well
    char number;
    bool vulnerable = false; // Flag for vulnerability
    bool hasSpeedBoost = false; // Flag for speed boost
    bool hasGhostPellet = false; // Flag for eating ghost pellet
};

std::vector<Ghost> ghosts = {
    {10, 10, '1'}, // Ghost 1
    {2, 10, '2'},  // Ghost 2
    {10, 15, '3'}, // Ghost 3
    {15, 5, '4'}   // Ghost 4
};

std::random_device rd;
#include <X11/Xlib.h>  
std::mt19937 eng(rd());

// Mutexes for thread synchronization
std::mutex gameBoardMutex;
std::mutex scoreMutex;
std::mutex livesMutex;
std::mutex speedBoostMutex;
std::condition_variable speedBoostCV;
int availableSpeedBoosts = 2; 
std::mutex movementMutex;
std::mutex powerPelletMutex;
std::mutex ghostHouseMutex;
std::condition_variable ghostHouseCV;
int availableKeys = 1;
int availableExitPermits = 1;

std::array<std::string, MAP_HEIGHT> map_sketch = {
    " ###################                     ",
    " #........#........#                     ",
    " #.##.###.#.###.##o#                     ",
    " #.................#                     ",
    " #.##.#.#####.#.##.#                     ",
    " #....#...#...#....#                     ",
    " ####.### # ###.####                     ",
    "    #.#   0   #.#                        ",
    "#####.# #   # #.#####                    ",
    "#    .  #   #  .    #                    ",
    "#    .  #   #  .    #                    ",
    "#####.# ##### #.#####                    ",
    "    #.#       #.#                        ",
    " ####.# ##### #.####                     ",
    " #........#........#                     ",
    " #.##.###.#.###.##.#                     ",
    " #..#.....P.....#..#                     ",
    " ##.#.###.#.###.#.##                     ",
    " #........#........#                     ",
    " #.##.###.#.###.##.#                     ",
    " #o................#                     ",
    " ###################                     ",
};

// Function prototypes
void handlingPM(sf::Keyboard::Key &lastDirection, int &pacmanX, int &pacmanY, std::array<std::array<Cell, MAP_WIDTH>, MAP_HEIGHT> &gameBoard, int &score);
void checkpmcollision(int &pacmanX, int &pacmanY, std::vector<Ghost> &ghosts, int &lives);
void GAmeover(sf::RenderWindow &window, int fs);
void Winnerwinnerchickendinner(sf::RenderWindow &window, int fs);
void drawGameElements(sf::RenderWindow &window, int x, int y, std::array<std::array<Cell, MAP_WIDTH>, MAP_HEIGHT> &gameBoard);
void ghostmovement(Ghost &ghost, std::mt19937 &eng);

sf::Color getGhostColor(char number, bool vulnerable);
void drawingghost(sf::RenderWindow &window, int x, int y, sf::Color color);
void drawPacman(sf::RenderWindow& window, int x, int y);
void powerpallet();
void depowerpallet();
void SpeedBoost(Ghost &ghost);
void releaseSpeedBoost(Ghost &ghost);

void *inputHandlingThread(void *arg);
void *gameStateUpdateThread(void *arg);
void *renderingThread(void *arg);
void *ghostThread(void *arg);
void *ghostPelletThread(void *arg); // Thread to manage the ghost pellet
void *uiThread(void *arg); // UI thread

int main() {  

sf::RenderWindow window(sf::VideoMode(1100,700), "PACMAN BY FAZZ AND HEER   ");
       sf::Texture texture;
    if (!texture.loadFromFile("startpic.png")) {
        return -1;
    }
    sf::Sprite sprite;
    sprite.setTexture(texture);
    float x = 0.0f;
    float y = 0.0f;

    // Main loop
    while (window.isOpen()) {
        sf::Event event;
        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed) {
                window.close();
            }
            if (event.type == sf::Event::KeyPressed) {
                if (event.key.code == sf::Keyboard::Space) {
                    window.close();
                } else if (event.key.code == sf::Keyboard::Enter) {
                    x += 1000.0f;
                }
            }
        }
        sprite.setPosition(x, y);
        window.clear();
        window.draw(sprite);
        window.display();
    }
    XInitThreads();
    for (int y = 0; y < MAP_HEIGHT; ++y) {
        for (int x = 0; x < MAP_WIDTH; ++x) {
            switch (map_sketch[y][x]) {
                case '#':
                    gameBoard[y][x].type = CellType::Wall;
                    break;
                case '.':
                    gameBoard[y][x].type = CellType::Pellet;
                    break;
                case 'o':
                    gameBoard[y][x].type = CellType::PowerPellet;
                    break;
                case 'P':
                    gameBoard[y][x].type = CellType::Pacman;
                    pacmanX = x;
                    pacmanY = y;
                    break;
                default:
                    gameBoard[y][x].type = CellType::Path;
                    break;
            }
        }
    }
    sf::Font font;
    if (!font.loadFromFile("arial.ttf")) {
        std::cerr << "Failed to load font arial.ttf. Ensure the file is in the correct directory." << std::endl;
        return -1;
    }

    // Initialize score and lives text
    scoreText.setFont(font);
    scoreText.setCharacterSize(24);
    scoreText.setFillColor(sf::Color::White);
    scoreText.setPosition(900, 300); 
    livesText.setFont(font);
    livesText.setCharacterSize(24);
    livesText.setFillColor(sf::Color::White);
    livesText.setPosition(905, 330);

    // Create threads
    pthread_t inputThread, gameStateThread, renderThread, ghostPelletThreadHandle, uiThreadHandle;
    pthread_t ghostThreads[ghosts.size()];

    pthread_create(&inputThread, NULL, inputHandlingThread, NULL);
    pthread_create(&gameStateThread, NULL, gameStateUpdateThread, NULL);
    pthread_create(&renderThread, NULL, renderingThread, NULL);
    pthread_create(&ghostPelletThreadHandle, NULL, ghostPelletThread, NULL); // Create ghost pellet thread
    pthread_create(&uiThreadHandle, NULL, uiThread, NULL); // Create UI thread

    for (size_t i = 0; i < ghosts.size(); ++i) {
        pthread_create(&ghostThreads[i], NULL, ghostThread, &ghosts[i]);
    }

    // Main game loop
    while (true) {
        if (totalPellets == 0 || lives <= 0) {
            break;
        }

        // Check if power pellet effect has ended
        if (powerPelletActive && std::chrono::steady_clock::now() > powerPelletEndTime) {
            depowerpallet();
        }
    }

    // Join threads
    pthread_join(inputThread, NULL);
    pthread_join(gameStateThread, NULL);
    pthread_join(renderThread, NULL);
    pthread_join(ghostPelletThreadHandle, NULL); // Join ghost pellet thread
    pthread_join(uiThreadHandle, NULL); // Join UI thread

    for (pthread_t &ghostThread : ghostThreads) {
        pthread_join(ghostThread, NULL);
    }

    return 0;
}

void *inputHandlingThread(void *arg) {
    while (true) {
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Left)) lastDirection = sf::Keyboard::Left;
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Right)) lastDirection = sf::Keyboard::Right;
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Up)) lastDirection = sf::Keyboard::Up;
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Down)) lastDirection = sf::Keyboard::Down;

        sf::sleep(sf::milliseconds(100));
    }
    return NULL;
}

void *gameStateUpdateThread(void *arg) {
    while (lives > 0) {
        {
            std::lock_guard<std::mutex> lock(gameBoardMutex);
            handlingPM(lastDirection, pacmanX, pacmanY, gameBoard, score);
            checkpmcollision(pacmanX, pacmanY, ghosts, lives);
        }

        sf::sleep(sf::milliseconds(200)); // Update game state every 200ms  
    }
    return NULL;
}

void *renderingThread(void *arg) {
    sf::RenderWindow window(sf::VideoMode(MAP_WIDTH * TILE_SIZE, MAP_HEIGHT * TILE_SIZE), "PACMAN BY HEER");

    while (window.isOpen()) {
        sf::Event event;
        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed)
                window.close();
        }

        window.clear(sf::Color::Black);

        {
            std::lock_guard<std::mutex> lock(gameBoardMutex);
            for (int y = 0; y < MAP_HEIGHT; ++y) {
                for (int x = 0; x < MAP_WIDTH; ++x) {
                    drawGameElements(window, x, y, gameBoard);
                }
            }

            for (const auto& ghost : ghosts) {
                sf::Color ghostColor = getGhostColor(ghost.number, ghost.vulnerable);
                drawingghost(window, ghost.x, ghost.y, ghostColor);
            }

            drawPacman(window, pacmanX, pacmanY);

            scoreText.setString("Score: " + std::to_string(score));
            livesText.setString("Lives: " + std::to_string(lives));
            window.draw(scoreText);
            window.draw(livesText);
        }

        if (lives <= 0) {
            GAmeover(window, score);
            window.close();
        }

        if (totalPellets == 0) {
            Winnerwinnerchickendinner(window, score);
            window.close();
        }

        window.display();
        sf::sleep(sf::milliseconds(200)); // Game speed 
    }
    return NULL;
}

void *ghostThread(void *arg) {
    Ghost *ghost = static_cast<Ghost*>(arg);
    while (lives > 0) {
        {
            std::unique_lock<std::mutex> lock(ghostHouseMutex);
            ghostHouseCV.wait(lock, [] { return availableKeys > 0 && availableExitPermits > 0; });
            availableKeys--;
            availableExitPermits--;
            lock.unlock();

            {
                std::lock_guard<std::mutex> lock(gameBoardMutex);
                ghostmovement(*ghost, eng);
                //  if ghost eats the ghost pellet
                if (ghost->x == ghostPelletX && ghost->y == ghostPelletY && ghostPelletAvailable) {
                    ghost->hasGhostPellet = true;
                    ghostsEatenGhostPellet++;
                    ghostPelletAvailable = false; // Ghost pellet disappear
                    if (ghostsEatenGhostPellet >= 2) {
                        ghost->hasSpeedBoost = true;
                    }
                }
            }

            lock.lock();
            availableKeys++;
            availableExitPermits++;
            ghostHouseCV.notify_one();
        }
        // Adjust ghost speed
        if (ghost->hasSpeedBoost) {
            sf::sleep(sf::milliseconds(50)); // Speed up ghost (4x faster)
        } else {
            sf::sleep(sf::milliseconds(200)); // Normal speed
        }
    }
    return NULL;
}

void *ghostPelletThread(void *arg) {
    sf::Clock clock;
    while (true) {
        if (clock.getElapsedTime().asSeconds() >= GHOST_PELLET_SPAWN_TIME && !ghostPelletAvailable) {
            std::lock_guard<std::mutex> lock(gameBoardMutex);
            ghostPelletAvailable = true;
            gameBoard[ghostPelletY][ghostPelletX].type = CellType::GhostPellet;
        }
        sf::sleep(sf::milliseconds(1000)); // Check every second
    }
    return NULL;
}

void *uiThread(void *arg) {
    while (true) {
        
        sf::sleep(sf::milliseconds(100)); 
    }
    return NULL;
}

void checkpmcollision(int &pacmanX, int &pacmanY, std::vector<Ghost> &ghosts, int &lives) {
    std::lock_guard<std::mutex> lock(powerPelletMutex);
    for (auto& ghost : ghosts) {
        if (pacmanX == ghost.x && pacmanY == ghost.y) {
            if (ghost.vulnerable) {
                // Eat the ghost
                score += 10;
                ghost.x = 10; 
                ghost.y = 10;
                ghost.vulnerable = false;
            } else {
                lives--;
                if (lives > 0) {
                 \
                    gameBoard[pacmanY][pacmanX].type = CellType::Path; // Clear  position
                    pacmanX = 9;
                    pacmanY = 16;
                    gameBoard[pacmanY][pacmanX].type = CellType::Pacman; // Set position
                }
                break;
            }
        }
    }
}

void handlingPM(sf::Keyboard::Key &lastDirection, int &pacmanX, int &pacmanY, std::array<std::array<Cell, MAP_WIDTH>, MAP_HEIGHT> &gameBoard, int &score) {
    std::lock_guard<std::mutex> lock(movementMutex);
    int newX = pacmanX, newY = pacmanY;
    if (lastDirection == sf::Keyboard::Left) newX--;
    if (lastDirection == sf::Keyboard::Right) newX++;
    if (lastDirection == sf::Keyboard::Up) newY--;
    if (lastDirection == sf::Keyboard::Down) newY++;

    if (gameBoard[newY][newX].type != CellType::Wall) {
        gameBoard[pacmanY][pacmanX].type = CellType::Path;
        pacmanX = newX;
        pacmanY = newY;

        if (gameBoard[pacmanY][pacmanX].type == CellType::Pellet) {
            score++;
            totalPellets--;
        } else if (gameBoard[pacmanY][pacmanX].type == CellType::PowerPellet) {
            score += 10; 
            powerpallet();
        }

        gameBoard[pacmanY][pacmanX].type = CellType::Pacman;
    }
}

void powerpallet() {
    powerPelletActive = true;
    powerPelletEndTime = std::chrono::steady_clock::now() + std::chrono::milliseconds(POWER_PELLET_DURATION);

    for (auto &ghost : ghosts) {
        ghost.vulnerable = true;
    }
}

void depowerpallet() {
    powerPelletActive = false;

    for (auto &ghost : ghosts) {
        ghost.vulnerable = false;
    }
}

void SpeedBoost(Ghost &ghost) {
    std::unique_lock<std::mutex> lock(speedBoostMutex);
    speedBoostCV.wait(lock, [] { return availableSpeedBoosts > 0; });
    availableSpeedBoosts--;
    ghost.hasSpeedBoost = true;
}

void releaseSpeedBoost(Ghost &ghost) {
    std::lock_guard<std::mutex> lock(speedBoostMutex);
    availableSpeedBoosts++;
    ghost.hasSpeedBoost = false;
    speedBoostCV.notify_one();
}

void GAmeover(sf::RenderWindow &window, int fs) {
    sf::Font font;
    if (!font.loadFromFile("arial.ttf")) {
        std::cerr << "Failed to load font arial.ttf. Ensure the file is in the correct directory." << std::endl;
        return;
    }

    sf::Text gameOverText("Game Over!\nFinal Score: " + std::to_string(fs), font, 50);
    gameOverText.setFillColor(sf::Color::Red);
    gameOverText.setStyle(sf::Text::Bold);

    sf::FloatRect textRect = gameOverText.getLocalBounds();
    gameOverText.setOrigin(textRect.left + textRect.width / 2.0f, textRect.top + textRect.height / 2.0f);
    gameOverText.setPosition(sf::Vector2f(window.getSize().x / 2.0f, window.getSize().y / 2.0f));

    window.clear();
    window.draw(gameOverText);
    window.display();

    sf::sleep(sf::seconds(5)); 
}

void Winnerwinnerchickendinner(sf::RenderWindow &window, int fs) {
    sf::Font font;
    if (!font.loadFromFile("arial.ttf")) {
        std::cerr << "Failed to load font arial.ttf. Ensure the file is in the correct directory." << std::endl;
        return;
    }

    sf::Text youWonText("You Won!\nFinal Score: " + std::to_string(fs), font, 50);
    youWonText.setFillColor(sf::Color::Green);
    youWonText.setStyle(sf::Text::Bold);

    sf::FloatRect textRect = youWonText.getLocalBounds();
    youWonText.setOrigin(textRect.left + textRect.width / 2.0f, textRect.top + textRect.height / 2.0f);
    youWonText.setPosition(sf::Vector2f(window.getSize().x / 2.0f, window.getSize().y / 2.0f));

    window.clear();
    window.draw(youWonText);
    window.display();

    sf::sleep(sf::seconds(5)); 
}

void drawGameElements(sf::RenderWindow &window, int x, int y, std::array<std::array<Cell, MAP_WIDTH>, MAP_HEIGHT> &gameBoard) {
    sf::RectangleShape rectangle(sf::Vector2f(TILE_SIZE, TILE_SIZE));
    rectangle.setPosition(x * TILE_SIZE, y * TILE_SIZE);

    sf::CircleShape pellet(TILE_SIZE / 6);
    pellet.setPosition((x + 0.5f) * TILE_SIZE - pellet.getRadius(), (y + 0.5f) * TILE_SIZE - pellet.getRadius());

    sf::CircleShape powerPellet(TILE_SIZE / 3);
    powerPellet.setPosition((x + 0.5f) * TILE_SIZE - powerPellet.getRadius(), (y + 0.5f) * TILE_SIZE - powerPellet.getRadius());

    sf::CircleShape ghostPellet(TILE_SIZE / 3);
    ghostPellet.setPosition((x + 0.5f) * TILE_SIZE - ghostPellet.getRadius(), (y + 0.5f) * TILE_SIZE - ghostPellet.getRadius());

    switch (gameBoard[y][x].type) {
        case CellType::Wall:
            rectangle.setFillColor(sf::Color::Magenta);
            window.draw(rectangle);
            break;
        case CellType::Path:
            rectangle.setFillColor(sf::Color::Black);
            window.draw(rectangle);
            break;
        case CellType::Pellet:
            rectangle.setFillColor(sf::Color::Black);
            window.draw(rectangle);
            pellet.setFillColor(sf::Color::Yellow);
            window.draw(pellet);
            break;
        case CellType::PowerPellet:
            rectangle.setFillColor(sf::Color::Black);
            window.draw(rectangle);
            powerPellet.setFillColor(sf::Color::Green);
            window.draw(powerPellet);
            break;
        case CellType::GhostPellet:
            rectangle.setFillColor(sf::Color::Black);
            window.draw(rectangle);
            ghostPellet.setFillColor(sf::Color::Red);
            window.draw(ghostPellet);
            break;
        case CellType::Pacman: {
            rectangle.setFillColor(sf::Color::Black);
            window.draw(rectangle);
            sf::CircleShape pacman(TILE_SIZE / 3);
            pacman.setFillColor(sf::Color::Yellow);
            pacman.setPosition(x * TILE_SIZE + TILE_SIZE / 3, y * TILE_SIZE + TILE_SIZE / 3);
            window.draw(pacman);
            break;
        }
        case CellType::Ghost:
            rectangle.setFillColor(sf::Color::Black);
            window.draw(rectangle);
            // Ghost color and drawing logic should be handled elsewhere
            break;
    }
}

void drawPacman(sf::RenderWindow& window, int x, int y) {
    sf::CircleShape pacman(TILE_SIZE / 3);
    pacman.setFillColor(sf::Color::Yellow);
    pacman.setPosition(x * TILE_SIZE + TILE_SIZE / 3, y * TILE_SIZE + TILE_SIZE / 3);
    window.draw(pacman);
}

void ghostmovement(Ghost &ghost, std::mt19937 &eng) {
    std::lock_guard<std::mutex> lock(movementMutex);
    std::uniform_int_distribution<> distr(0, 3);

    int newX = ghost.x + ghost.dx;
    int newY = ghost.y + ghost.dy;

    if (newX < 0 || newX >= MAP_WIDTH || newY < 0 || newY >= MAP_HEIGHT || gameBoard[newY][newX].type == CellType::Wall) {
        while (true) {
            int direction = distr(eng);
            ghost.dx = 0;
            ghost.dy = 0;
            switch (direction) {
                case 0: ghost.dy = -1; break;
                case 1: ghost.dy = 1; break;
                case 2: ghost.dx = -1; break;
                case 3: ghost.dx = 1; break;
            }

            newX = ghost.x + ghost.dx;
            newY = ghost.y + ghost.dy;

            if (newX >= 0 && newX < MAP_WIDTH && newY >= 0 && newY < MAP_HEIGHT && gameBoard[newY][newX].type != CellType::Wall) {
                break;
            }
        }
    }

    ghost.x = newX;
    ghost.y = newY;
}

sf::Color getGhostColor(char number, bool vulnerable) {
    if (vulnerable) {
        return sf::Color::Blue;
    }
    switch (number) {
        case '1': return sf::Color::Red;
        case '2': return sf::Color::Blue;
        case '3': return sf::Color::Cyan;
        case '4': return sf::Color::Green; // Color for the fourth ghost
        default: return sf::Color::White;
    }
}

void drawingghost(sf::RenderWindow& window, int x, int y, sf::Color color) {
    sf::CircleShape head(TILE_SIZE / 4);
    head.setFillColor(color);
    head.setPosition(x * TILE_SIZE + TILE_SIZE / 4, y * TILE_SIZE + TILE_SIZE / 8);

    sf::RectangleShape body(sf::Vector2f(TILE_SIZE / 2, TILE_SIZE / 2));
    body.setFillColor(color);
    body.setPosition(x * TILE_SIZE + TILE_SIZE / 4, y * TILE_SIZE + TILE_SIZE * 3 / 8);

    window.draw(head);
    window.draw(body);
}
