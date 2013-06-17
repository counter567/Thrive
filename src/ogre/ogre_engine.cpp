#include "ogre/ogre_engine.h"

#include "bullet/bullet_engine.h"
#include "bullet/debug_drawing.h"
#include "game.h"
#include "ogre/camera_system.h"
#include "ogre/entity_system.h"
#include "ogre/keyboard_system.h"
#include "ogre/light_system.h"
#include "ogre/render_system.h"
#include "ogre/scene_node_system.h"
#include "ogre/sky_system.h"
#include "ogre/viewport_system.h"

#include <boost/lexical_cast.hpp>
#include <OgreConfigFile.h>
#include <OgreLogManager.h>
#include <OgreRenderWindow.h>
#include <OgreRoot.h>
#include <OgreWindowEventUtilities.h>
#include <OISInputManager.h>
#include <OISMouse.h>
#include <stdlib.h>

#include <iostream>

using namespace thrive;

#ifdef _DEBUG
    static const char* RESOURCES_CFG = "resources_d.cfg";
    static const char* PLUGINS_CFG   = "plugins_d.cfg";
#else
    static const char* RESOURCES_CFG = "resources.cfg";
    static const char* PLUGINS_CFG   = "plugins.cfg";
#endif

struct OgreEngine::Implementation : public Ogre::WindowEventListener {

    Implementation()
      : m_keyboardSystem(new KeyboardSystem()),
        m_viewportSystem(new OgreViewportSystem())
    {
    }

    ~Implementation() {
        Ogre::WindowEventUtilities::removeWindowEventListener(
            m_window,
            this
        );
    }

    void
    loadConfig() {
        if(!(m_root->restoreConfig() or m_root->showConfigDialog()))
        {
            exit(EXIT_SUCCESS);
        }
    }

    void
    loadResources() {
        Ogre::ConfigFile config;
        config.load(RESOURCES_CFG);
        auto sectionIter = config.getSectionIterator();
        auto& resourceManager = Ogre::ResourceGroupManager::getSingleton();
        while (sectionIter.hasMoreElements()) {
            std::string sectionName = sectionIter.peekNextKey();
            Ogre::ConfigFile::SettingsMultiMap* sectionContent = sectionIter.getNext();
            for(auto& setting : *sectionContent) {
                std::string resourceType = setting.first;
                std::string resourceLocation = setting.second;
                resourceManager.addResourceLocation(
                    resourceLocation,
                    resourceType,
                    sectionName
                );
            }
        }
    }

    void
    setupInputManager() {
        const std::string HANDLE_NAME = "WINDOW";
        size_t windowHandle = 0;
        m_window->getCustomAttribute(HANDLE_NAME, &windowHandle);
        OIS::ParamList parameters;
        parameters.insert(std::make_pair(
            HANDLE_NAME,
            boost::lexical_cast<std::string>(windowHandle)
        ));
        m_inputManager = OIS::InputManager::createInputSystem(parameters);
    }

    void
    setupLighting() {
        m_sceneManager->setAmbientLight(Ogre::ColourValue(0.5, 0.5, 0.5));
    }

    void
    setupLog() {
        static auto logManager = new Ogre::LogManager();
        logManager->createLog("default", true, false, false);
    }

    void
    setupSceneManager() {
        m_sceneManager = m_root->createSceneManager("DefaultSceneManager");
    }

    void
    shutdownInputManager() {
        if (not m_inputManager) {
            return;
        }
        OIS::InputManager::destroyInputSystem(m_inputManager);
        m_inputManager = nullptr;
    }

    bool 
    windowClosing(
        Ogre::RenderWindow* window
    ) override {
        if (window == m_window) {
            Game::instance().quit();
        }
        return true;
    }

    std::unique_ptr<Ogre::Root> m_root;

    OIS::InputManager* m_inputManager = nullptr;

    std::shared_ptr<KeyboardSystem> m_keyboardSystem;

    Ogre::SceneManager* m_sceneManager = nullptr;

    std::shared_ptr<OgreViewportSystem> m_viewportSystem;

    Ogre::RenderWindow* m_window = nullptr;

};


OgreEngine::OgreEngine(
    EntityManager& entityManager
) : Engine(entityManager),
    m_impl(new Implementation())
{
}


OgreEngine::~OgreEngine() {}


void
OgreEngine::init() {
    m_impl->setupLog();
    m_impl->m_root.reset(new Ogre::Root(PLUGINS_CFG));
    m_impl->loadResources();
    m_impl->loadConfig();
    m_impl->m_window = m_impl->m_root->initialise(true, "Thrive");
    Ogre::WindowEventUtilities::addWindowEventListener(
        m_impl->m_window,
        m_impl.get()
    );
    // Set default mipmap level (NB some APIs ignore this)
    Ogre::TextureManager::getSingleton().setDefaultNumMipmaps(5);
    // initialise all resource groups
    Ogre::ResourceGroupManager::getSingleton().initialiseAllResourceGroups();
    // Setup
    m_impl->setupSceneManager();
    m_impl->setupLighting();
    m_impl->setupInputManager();
    // Create essential systems
    this->addSystem(
        m_impl->m_keyboardSystem
    );
    this->addSystem(
        std::make_shared<OgreAddSceneNodeSystem>()
    );
    this->addSystem(
        std::make_shared<OgreUpdateSceneNodeSystem>()
    );
    this->addSystem(
        std::make_shared<OgreCameraSystem>()
    );
    this->addSystem(
        std::make_shared<OgreLightSystem>()
    );
    this->addSystem(
        std::make_shared<SkySystem>()
    );
    this->addSystem(
        std::make_shared<OgreEntitySystem>()
    );
    this->addSystem(
        m_impl->m_viewportSystem // Has to come *after* camera system
    );
    this->addSystem(
        std::make_shared<OgreRemoveSceneNodeSystem>()
    );
    this->addSystem(
        std::make_shared<RenderSystem>()
    );
    Engine::init();
}


OIS::InputManager*
OgreEngine::inputManager() const {
    return m_impl->m_inputManager;
}


std::shared_ptr<KeyboardSystem>
OgreEngine::keyboardSystem() const {
    return m_impl->m_keyboardSystem;
}


Ogre::Root*
OgreEngine::root() const {
    return m_impl->m_root.get();
}


Ogre::SceneManager*
OgreEngine::sceneManager() const {
    return m_impl->m_sceneManager;
}


void
OgreEngine::shutdown() {
    Engine::shutdown();
    m_impl->shutdownInputManager();
    m_impl->m_window->destroy();
    m_impl->m_root.reset();
}


void
OgreEngine::update(int milliSeconds) {
    // Handle events
    Ogre::WindowEventUtilities::messagePump();
    // Update systems
    Engine::update(milliSeconds);
}


OgreViewportSystem&
OgreEngine::viewportSystem() {
    return *(m_impl->m_viewportSystem);
}


Ogre::RenderWindow*
OgreEngine::window() const {
    return m_impl->m_window;
}


