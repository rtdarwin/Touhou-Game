﻿#ifdef WIN32
#pragma execution_character_set("utf-8")
#endif

#include "GameplayScene/GameplayScene.h"
#include "GameplayScene/CtrlPanel/CtrlPanelLayer.h"
#include "GameplayScene/Elevator.h"
#include "GameplayScene/Emitters/Bullet.h"
#include "GameplayScene/Emitters/Emitter.h"
#include "GameplayScene/Enemy/Enemy.h"
#include "GameplayScene/EventFilterManager.h"
#include "GameplayScene/EventScriptHanding.h"
#include "GameplayScene/Player/Player.h"
#include "GameplayScene/common.h"

#include "Layers/ConversationLayer.h"
#include "Layers/SettingsLayer.h"

#include "AudioController.h"

#define PTM_RATIO 1

#define BACK_PARALLAX_ZORDER -10
#define FORE_PARALLAX_ZORDER 10

#define CTRLPANEL_LAYER_ZORDER 100
#define SETTING_LAYER_ZORDER 1000
#define MAP_LAYER_ZORDER 0

#define MAP_LAYER_TMXMAP_ZORDER 0
#define MAP_LAYER_CHARACTER_ZORDER 1
#define MAP_LAYER_ENEMY_ZORDER 1
#define MAP_LAYER_CAMERA_ZORDER -1
#define MAP_LAYER_OTHER_ZORDER 2

const std::string GameplayScene::TAG{ "GameplayScene" };

void
GameplayScene::onEnter()
{
    Scene::onEnter();
}

void
GameplayScene::onEnterTransitionDidFinish()
{
    Scene::onEnterTransitionDidFinish();
}

void
GameplayScene::onExit()
{
    Scene::onExit();
}

void
GameplayScene::cleanup()
{
    Scene::cleanup();
    AudioController::getInstance()->clearMusic();
    Director::getInstance()->getEventDispatcher()->removeEventListenersForTarget(this);
    _eventFilterMgr->removeAllEventFilters();

    AnimationCache::getInstance()->destroyInstance();
    p1Player->release();
    p2Player->release();
}

GameplayScene*
GameplayScene::create(const std::string& map)
{
    GameplayScene* pRet = new (std::nothrow) GameplayScene(map);
    if (pRet && pRet->init()) {
        pRet->autorelease();
        return pRet;
    } else {
        delete pRet;
        pRet = nullptr;
        return nullptr;
    }
}

GameplayScene::GameplayScene(const std::string& map)
{
    selectedMap = map;
    visibleSize = Director::getInstance()->getVisibleSize();
}

GameplayScene::~GameplayScene()
{
    delete _eventScriptHanding;
}

bool
GameplayScene::init()
{
    if (!Scene::init()) {
        return false;
    }

    this->initWithPhysics();                      //初始化物理世界
    Vect gravity(0, -gameGravity);                //游戏场景的重力
    this->getPhysicsWorld()->setGravity(gravity); //设置重力
#ifndef NDEBUG
    this->getPhysicsWorld()->setDebugDrawMask(PhysicsWorld::DEBUGDRAW_ALL); //调试模式看包围盒
#endif

    // 用于支持符卡 buf 效果的 EventFilterManager
    this->_eventFilterMgr = EventFilterManager::create();
    this->_eventFilterMgr->retain();

    _eventScriptHanding = new EventScriptHanding(this);

    return true;
}

void
GameplayScene::initBackgroundAndForeground()
{
    //视差背景
    backgroundParallaxNode = ParallaxNode::create();
    this->addChild(backgroundParallaxNode, BACK_PARALLAX_ZORDER);
    backgroundParallaxNode->setPosition(0, 0);
    //背景图片，锚点为图片中心点
    backgroundParallaxPicture = Sprite::create();
    backgroundParallaxPicture->setAnchorPoint(Point::ANCHOR_MIDDLE);
    //加入视差结点，子节点位置只和偏移量，比率因子等有关
    //参数分别为：添加的子节点，父节点内Z轴顺序，移动比率，相对父节点偏移量
    backgroundParallaxNode->addChild(backgroundParallaxPicture, -10, Vec2(0.06, 0.03),
                                     Vec2(visibleSize.width / 2.0, visibleSize.height / 2.0));
    //视差背景装饰物层
    backgroundParallaxDecoration = Layer::create();
    backgroundParallaxDecoration->setAnchorPoint(Point::ZERO);
    backgroundParallaxNode->addChild(backgroundParallaxDecoration, -5, Vec2(0.20, 0.9), Vec2(0, 0));
    //静态背景装饰物层，相对视差结点同步移动
    backgroundStaticDecoration = Layer::create();
    backgroundStaticDecoration->setAnchorPoint(Point::ZERO);
    backgroundParallaxNode->addChild(backgroundStaticDecoration, 0, Vec2(1.0, 1.0), Vec2(0, 0));

    //视差前景
    foregroundParallaxNode = ParallaxNode::create();
    this->addChild(foregroundParallaxNode, FORE_PARALLAX_ZORDER);
    foregroundParallaxNode->setPosition(0, 0);
    //静态前景装饰物层
    foregroundStaticDecoration = Layer::create();
    foregroundStaticDecoration->setAnchorPoint(Point::ZERO);
    foregroundParallaxNode->addChild(foregroundStaticDecoration, 0, Vec2(1.0, 1.0), Vec2(0, 0));
    //视差前景装饰物层
    foregroundParallaxDecoration = Layer::create();
    foregroundParallaxDecoration->setAnchorPoint(Point::ZERO);
    foregroundParallaxNode->addChild(foregroundParallaxDecoration, 5, Vec2(0.20, 0.9), Vec2(0, 0));
}

void
GameplayScene::initMap()
{
    mapLayer = Layer::create();
    _map = TMXTiledMap::create(selectedMap);
    //设置地图大小的倍率
    _map->setScale(1.0f);
    mapLayer->addChild(_map, MAP_LAYER_TMXMAP_ZORDER);
    mapLayer->setName("mapLayer");
    this->addChild(mapLayer, MAP_LAYER_ZORDER);

    //创建静态刚体墙
    createPhysical(1);
}

//创建静态刚体，接受参数设置刚体大小倍率
bool
GameplayScene::createPhysical(float scale)
{
    // 找出阻挡区域所在的层
    TMXObjectGroup* group = _map->getObjectGroup("physics");
    auto objects = group->getObjects();
    // 在控制台输出对象信息
    // Value objectsVal = Value(objects);
    // log("%s", objectsVal.getDescription().c_str());

    for (auto& v : objects) {
        auto dict = v.asValueMap();

        if (dict.size() == 0)
            continue;

        // 读取所有形状的起始点
        float x = dict["x"].asFloat() * scale;
        float y = dict["y"].asFloat() * scale;

        //多边形polygonPoints
        if (dict.find("points") != dict.end()) {
            auto polygon_points = dict["points"].asValueVector();

            Vec2 points[20];
            int shapeVecAmount = 0; //每个shape的顶点个数

            // 必须将所有读取的定点逆向，因为翻转y之后，三角形定点的顺序已经逆序了，构造PolygonShape会crash
            int polygonSize = polygon_points.size();
            polygon_points.resize(polygonSize);
            polygonSize--;

            for (auto obj : polygon_points) {
                // 相对于起始点的偏移
                float offx = obj.asValueMap()["x"].asFloat() * scale;
                float offy = obj.asValueMap()["y"].asFloat() * scale;

                points[polygonSize] = Vec2((x + offx) / PTM_RATIO, (y - offy) / PTM_RATIO);
                polygonSize--;
                shapeVecAmount++;
            }

            PhysicsBody* _pBody = PhysicsBody::createPolygon(points, shapeVecAmount);
            _pBody->getFirstShape()->setDensity(0);
            _pBody->getFirstShape()->setFriction(1.0);
            _pBody->getFirstShape()->setRestitution(0);
            _pBody->setDynamic(false);
            _pBody->setCategoryBitmask(groundCategory); //给多边形地面设置掩码，默认值为0xFFFFFFFF
            _pBody->setCollisionBitmask(playerCategory | enemyCategory); //默认值为0xFFFFFFFF
            _pBody->setContactTestBitmask(playerCategory);               //默认值为0

            auto sprite = Sprite::create();
            sprite->setTag(polygonCategoryTag);
            sprite->setPhysicsBody(_pBody);
            mapLayer->addChild(sprite);
        } else if (dict.find("polylinePoints") != dict.end()) {
            auto polyline_points = dict["polylinePoints"].asValueVector();

            int shapeVecAmount = 0; //每个shape的顶点个数
            Vec2 points[20];

            int i = 0;
            for (auto obj : polyline_points) {
                float offx = obj.asValueMap()["x"].asFloat() * scale;
                float offy = obj.asValueMap()["y"].asFloat() * scale;

                points[i] = Vec2((x + offx) / PTM_RATIO, (y - offy) / PTM_RATIO);
                i++;
                shapeVecAmount++;
            }

            PhysicsBody* _pBody = PhysicsBody::createEdgeChain(points, shapeVecAmount);
            _pBody->getFirstShape()->setDensity(0);
            _pBody->getFirstShape()->setFriction(1.0);
            _pBody->getFirstShape()->setRestitution(0);
            _pBody->setDynamic(false);
            _pBody->setCategoryBitmask(groundCategory); //给折线地面设置掩码，默认值为0xFFFFFFFF
            _pBody->setCollisionBitmask(playerCategory | enemyCategory);   //默认值为0xFFFFFFFF
            _pBody->setContactTestBitmask(playerCategory | enemyCategory); //默认值为0

            auto sprite = Sprite::create();
            sprite->setPhysicsBody(_pBody);
            sprite->setTag(polylineCategoryTag);
            mapLayer->addChild(sprite);
        } else {
            PhysicsBody* _pBody;

            float width = dict["width"].asFloat() * scale;
            float height = dict["height"].asFloat() * scale;

            _pBody = PhysicsBody::createBox(Size(width, height));
            _pBody->getFirstShape()->setDensity(0);
            _pBody->getFirstShape()->setFriction(1.0);
            _pBody->getFirstShape()->setRestitution(0);
            _pBody->setDynamic(false);
            _pBody->setCategoryBitmask(groundCategory); //给矩形地面设置掩码，默认值为0xFFFFFFFF
            _pBody->setCollisionBitmask(playerCategory | enemyCategory); //默认值为0xFFFFFFFF
            _pBody->setContactTestBitmask(playerCategory | enemyCategory |
                                          elevatorCategory); //默认值为0

            auto sprite = Sprite::create();
            sprite->setTag(groundCategoryTag);
            sprite->setPosition(x + width / 2.0f, y + height / 2.0f);
            sprite->setPhysicsBody(_pBody);
            mapLayer->addChild(sprite);
        }
    }
    return true;
}

#define CREATE_AND_ADD_ANIMATION_CACHE(frames, delayPerUnit, key)                                  \
    if (frames.size() > 0) {                                                                       \
        auto animation = Animation::create();                                                      \
        for (auto& v : frames) {                                                                   \
            animation->addSpriteFrameWithFile(v);                                                  \
        }                                                                                          \
        animation->setDelayPerUnit(delayPerUnit);                                                  \
        AnimationCache::getInstance()->addAnimation(animation, key);                               \
    }

void
GameplayScene::initAnimationCache()
{
    //初始化子弹素材
    SpriteFrameCache::getInstance()->addSpriteFramesWithFile("emitter/bullets/bullet1.plist");
    SpriteFrameCache::getInstance()->addSpriteFramesWithFile("emitter/bullets/bullet2.plist");
    SpriteFrameCache::getInstance()->addSpriteFramesWithFile("emitter/bullets/bullet3.plist");
    SpriteFrameCache::getInstance()->addSpriteFramesWithFile("emitter/bullets/laser1.plist");

    auto characterTags = GameData::getInstance()->getOnStageCharacterTagList();

    std::set<string> enemyTags;
    TMXObjectGroup* group = _map->getObjectGroup("enemy");
    auto objects = group->getObjects();
    for (auto v : objects) {
        auto dict = v.asValueMap();
        if (dict.size() == 0)
            continue;
        std::string tag = dict["tag"].asString();
        enemyTags.insert(tag);
    }

    for (auto& s : characterTags) {
        Character _character = GameData::getInstance()->getCharacterByTag(s);

        CREATE_AND_ADD_ANIMATION_CACHE(_character.standFrame, _character.standFrameDelay,
                                       _character.standAnimationKey);
        CREATE_AND_ADD_ANIMATION_CACHE(_character.runFrame, _character.runFrameDelay,
                                       _character.runAnimationKey);
        CREATE_AND_ADD_ANIMATION_CACHE(_character.preJumpFrame, _character.preJumpFrameDelay,
                                       _character.preJumpAnimationKey);
        CREATE_AND_ADD_ANIMATION_CACHE(_character.jumpFrame, _character.jumpFrameDelay,
                                       _character.jumpAnimationKey);
        CREATE_AND_ADD_ANIMATION_CACHE(_character.preFallFrame, _character.preFallFrameDelay,
                                       _character.preFallAnimationKey);
        CREATE_AND_ADD_ANIMATION_CACHE(_character.fallFrame, _character.fallFrameDelay,
                                       _character.fallAnimationKey);
        CREATE_AND_ADD_ANIMATION_CACHE(_character.dashFrame, _character.dashFrameDelay,
                                       _character.dashAnimationKey);
        CREATE_AND_ADD_ANIMATION_CACHE(_character.useSpellCardFrame,
                                       _character.useSpellCardFrameDelay,
                                       _character.useSpellCardAnimationKey);
    }

    for (auto& s : enemyTags) {
        EnemyData _enemy = GameData::getInstance()->getEnemyByTag(s);

        CREATE_AND_ADD_ANIMATION_CACHE(_enemy.standFrame, _enemy.standFrameDelay,
                                       _enemy.standAnimationKey);
        CREATE_AND_ADD_ANIMATION_CACHE(_enemy.runFrame, _enemy.runFrameDelay,
                                       _enemy.runAnimationKey);
        CREATE_AND_ADD_ANIMATION_CACHE(_enemy.preJumpFrame, _enemy.preJumpFrameDelay,
                                       _enemy.preJumpAnimationKey);
        CREATE_AND_ADD_ANIMATION_CACHE(_enemy.jumpFrame, _enemy.jumpFrameDelay,
                                       _enemy.jumpAnimationKey);
        CREATE_AND_ADD_ANIMATION_CACHE(_enemy.preFallFrame, _enemy.preFallFrameDelay,
                                       _enemy.preFallAnimationKey);
        CREATE_AND_ADD_ANIMATION_CACHE(_enemy.fallFrame, _enemy.fallFrameDelay,
                                       _enemy.fallAnimationKey);
        CREATE_AND_ADD_ANIMATION_CACHE(_enemy.dashFrame, _enemy.dashFrameDelay,
                                       _enemy.dashAnimationKey);
        CREATE_AND_ADD_ANIMATION_CACHE(_enemy.hitFrame, _enemy.hitFrameDelay,
                                       _enemy.hitAnimationKey);
        CREATE_AND_ADD_ANIMATION_CACHE(_enemy.downFrame, _enemy.downFrameDelay,
                                       _enemy.downAnimationKey);
    }

    ///////////////////////////////////////////////

    auto use = Animation::create();
    use->addSpriteFrameWithFile("effect/superJump000.png");
    use->addSpriteFrameWithFile("effect/superJump001.png");
    use->addSpriteFrameWithFile("effect/superJump002.png");
    use->addSpriteFrameWithFile("effect/superJump003.png");
    use->addSpriteFrameWithFile("effect/superJump004.png");
    use->addSpriteFrameWithFile("effect/superJump005.png");
    use->addSpriteFrameWithFile("effect/superJump006.png");
    use->addSpriteFrameWithFile("effect/superJump007.png");
    use->addSpriteFrameWithFile("effect/superJump008.png");
    use->addSpriteFrameWithFile("effect/superJump009.png");
    use->addSpriteFrameWithFile("effect/superJump010.png");
    use->setDelayPerUnit(0.10);
    AnimationCache::getInstance()->addAnimation(use, "use");

    ///////////////////////////////////////////////

    auto sakuyaAttackA_1 = Animation::create();
    sakuyaAttackA_1->addSpriteFrameWithFile("character/Sakuya/shotAb000.png");
    sakuyaAttackA_1->addSpriteFrameWithFile("character/Sakuya/shotAb001.png");
    sakuyaAttackA_1->addSpriteFrameWithFile("character/Sakuya/shotAb002.png");
    sakuyaAttackA_1->addSpriteFrameWithFile("character/Sakuya/shotAb003.png");
    sakuyaAttackA_1->addSpriteFrameWithFile("character/Sakuya/shotAb004.png");
    sakuyaAttackA_1->addSpriteFrameWithFile("character/Sakuya/shotAb005.png");
    sakuyaAttackA_1->setDelayPerUnit(0.10);
    AnimationCache::getInstance()->addAnimation(sakuyaAttackA_1, "sakuyaAttackA_1");
    auto sakuyaAttackA_2 = Animation::create();
    sakuyaAttackA_2->addSpriteFrameWithFile("character/Sakuya/shotAb006.png");
    sakuyaAttackA_2->addSpriteFrameWithFile("character/Sakuya/shotAb007.png");
    sakuyaAttackA_2->addSpriteFrameWithFile("character/Sakuya/shotAb008.png");
    sakuyaAttackA_2->addSpriteFrameWithFile("character/Sakuya/shotAb009.png");
    sakuyaAttackA_2->addSpriteFrameWithFile("character/Sakuya/shotAb010.png");
    sakuyaAttackA_2->setDelayPerUnit(0.10);
    AnimationCache::getInstance()->addAnimation(sakuyaAttackA_2, "sakuyaAttackA_2");

    auto sakuyaAttackB_1 = Animation::create();
    sakuyaAttackB_1->addSpriteFrameWithFile("character/Sakuya/shotAa000.png");
    sakuyaAttackB_1->addSpriteFrameWithFile("character/Sakuya/shotAa001.png");
    sakuyaAttackB_1->addSpriteFrameWithFile("character/Sakuya/shotAa002.png");
    sakuyaAttackB_1->addSpriteFrameWithFile("character/Sakuya/shotAa003.png");
    sakuyaAttackB_1->addSpriteFrameWithFile("character/Sakuya/shotAa004.png");
    sakuyaAttackB_1->addSpriteFrameWithFile("character/Sakuya/shotAa005.png");
    sakuyaAttackB_1->setDelayPerUnit(0.10);
    AnimationCache::getInstance()->addAnimation(sakuyaAttackB_1, "sakuyaAttackB_1");
    auto sakuyaAttackB_2 = Animation::create();
    sakuyaAttackB_2->addSpriteFrameWithFile("character/Sakuya/shotAa006.png");
    sakuyaAttackB_2->addSpriteFrameWithFile("character/Sakuya/shotAa007.png");
    sakuyaAttackB_2->addSpriteFrameWithFile("character/Sakuya/shotAa008.png");
    sakuyaAttackB_2->addSpriteFrameWithFile("character/Sakuya/shotAa009.png");
    sakuyaAttackB_2->addSpriteFrameWithFile("character/Sakuya/shotAa010.png");
    sakuyaAttackB_2->setDelayPerUnit(0.10);
    AnimationCache::getInstance()->addAnimation(sakuyaAttackB_2, "sakuyaAttackB_2");

    auto sakuyaUseSpellCard = Animation::create();
    sakuyaUseSpellCard->addSpriteFrameWithFile("character/Sakuya/spellDa000.png");
    sakuyaUseSpellCard->addSpriteFrameWithFile("character/Sakuya/spellDa001.png");
    sakuyaUseSpellCard->addSpriteFrameWithFile("character/Sakuya/spellDa002.png");
    sakuyaUseSpellCard->addSpriteFrameWithFile("character/Sakuya/spellDa003.png");
    sakuyaUseSpellCard->addSpriteFrameWithFile("character/Sakuya/spellDa004.png");
    sakuyaUseSpellCard->addSpriteFrameWithFile("character/Sakuya/spellDa005.png");
    sakuyaUseSpellCard->addSpriteFrameWithFile("character/Sakuya/spellDa006.png");
    sakuyaUseSpellCard->addSpriteFrameWithFile("character/Sakuya/spellDa007.png");
    sakuyaUseSpellCard->addSpriteFrameWithFile("character/Sakuya/spellDa008.png");
    sakuyaUseSpellCard->addSpriteFrameWithFile("character/Sakuya/spellDa009.png");
    sakuyaUseSpellCard->setDelayPerUnit(0.10);
    AnimationCache::getInstance()->addAnimation(sakuyaUseSpellCard, "sakuyaUseSpellCard");

    //////////////////////////////////////

    auto udongeAttackAa_1 = Animation::create();
    udongeAttackAa_1->addSpriteFrameWithFile("character/Udonge/shotAa000.png");
    udongeAttackAa_1->addSpriteFrameWithFile("character/Udonge/shotAa001.png");
    udongeAttackAa_1->addSpriteFrameWithFile("character/Udonge/shotAa002.png");
    udongeAttackAa_1->addSpriteFrameWithFile("character/Udonge/shotAa003.png");
    udongeAttackAa_1->setDelayPerUnit(0.10);
    AnimationCache::getInstance()->addAnimation(udongeAttackAa_1, "udongeAttackAa_1");
    auto udongeAttackAa_2 = Animation::create();
    udongeAttackAa_2->addSpriteFrameWithFile("character/Udonge/shotAa004.png");
    udongeAttackAa_2->addSpriteFrameWithFile("character/Udonge/shotAa005.png");
    udongeAttackAa_2->setDelayPerUnit(0.15);
    AnimationCache::getInstance()->addAnimation(udongeAttackAa_2, "udongeAttackAa_2");

    auto udongeAttackBa_1 = Animation::create();
    udongeAttackBa_1->addSpriteFrameWithFile("character/Udonge/shotBa000.png");
    udongeAttackBa_1->addSpriteFrameWithFile("character/Udonge/shotBa001.png");
    udongeAttackBa_1->addSpriteFrameWithFile("character/Udonge/shotBa002.png");
    udongeAttackBa_1->addSpriteFrameWithFile("character/Udonge/shotBa003.png");
    udongeAttackBa_1->addSpriteFrameWithFile("character/Udonge/shotBa004.png");
    udongeAttackBa_1->setDelayPerUnit(0.10);
    AnimationCache::getInstance()->addAnimation(udongeAttackBa_1, "udongeAttackBa_1");
    auto udongeAttackBa_2 = Animation::create();
    udongeAttackBa_2->addSpriteFrameWithFile("character/Udonge/shotBa005.png");
    udongeAttackBa_2->addSpriteFrameWithFile("character/Udonge/shotBa006.png");
    udongeAttackBa_2->addSpriteFrameWithFile("character/Udonge/shotBa007.png");
    udongeAttackBa_2->setDelayPerUnit(0.10);
    AnimationCache::getInstance()->addAnimation(udongeAttackBa_2, "udongeAttackBa_2");

    auto udongeAttackBD_1 = Animation::create();
    udongeAttackBD_1->addSpriteFrameWithFile("character/Udonge/shotBd000.png");
    udongeAttackBD_1->addSpriteFrameWithFile("character/Udonge/shotBd001.png");
    udongeAttackBD_1->addSpriteFrameWithFile("character/Udonge/shotBd002.png");
    udongeAttackBD_1->addSpriteFrameWithFile("character/Udonge/shotBd003.png");
    udongeAttackBD_1->addSpriteFrameWithFile("character/Udonge/shotBd004.png");
    udongeAttackBD_1->setDelayPerUnit(0.10);
    AnimationCache::getInstance()->addAnimation(udongeAttackBD_1, "udongeAttackBd_1");
    auto udongeAttackBD_2 = Animation::create();
    udongeAttackBD_2->addSpriteFrameWithFile("character/Udonge/shotBd005.png");
    udongeAttackBD_2->addSpriteFrameWithFile("character/Udonge/shotBd006.png");
    udongeAttackBD_2->addSpriteFrameWithFile("character/Udonge/shotBd007.png");
    udongeAttackBD_2->setDelayPerUnit(0.10);
    AnimationCache::getInstance()->addAnimation(udongeAttackBD_2, "udongeAttackBd_2");

    auto udongeAttackAd_1 = Animation::create();
    udongeAttackAd_1->addSpriteFrameWithFile("character/Udonge/shotAd000.png");
    udongeAttackAd_1->addSpriteFrameWithFile("character/Udonge/shotAd001.png");
    udongeAttackAd_1->addSpriteFrameWithFile("character/Udonge/shotAd002.png");
    udongeAttackAd_1->addSpriteFrameWithFile("character/Udonge/shotAd003.png");
    udongeAttackAd_1->addSpriteFrameWithFile("character/Udonge/shotAd004.png");
    udongeAttackAd_1->addSpriteFrameWithFile("character/Udonge/shotAd005.png");
    udongeAttackAd_1->setDelayPerUnit(0.10);
    AnimationCache::getInstance()->addAnimation(udongeAttackAd_1, "udongeAttackAd_1");

    auto udongeUseSpellCard = Animation::create();
    udongeUseSpellCard->addSpriteFrameWithFile("character/Udonge/spellCall000.png");
    udongeUseSpellCard->addSpriteFrameWithFile("character/Udonge/spellCall001.png");
    udongeUseSpellCard->addSpriteFrameWithFile("character/Udonge/spellCall002.png");
    udongeUseSpellCard->addSpriteFrameWithFile("character/Udonge/spellCall003.png");
    udongeUseSpellCard->addSpriteFrameWithFile("character/Udonge/spellCall004.png");
    udongeUseSpellCard->addSpriteFrameWithFile("character/Udonge/spellCall005.png");
    udongeUseSpellCard->addSpriteFrameWithFile("character/Udonge/spellCall006.png");
    udongeUseSpellCard->addSpriteFrameWithFile("character/Udonge/spellCall007.png");
    udongeUseSpellCard->addSpriteFrameWithFile("character/Udonge/spellCall008.png");
    udongeUseSpellCard->setDelayPerUnit(0.10);
    AnimationCache::getInstance()->addAnimation(udongeUseSpellCard, "udongeUseSpellCard");

    //////////////////////////////////////

    SpriteFrameCache::getInstance()->addSpriteFramesWithFile("enemy/Stump.plist");
    auto stumpStand = Animation::create();
    stumpStand->addSpriteFrame(
        SpriteFrameCache::getInstance()->getSpriteFrameByName("stump_stand.png"));
    stumpStand->setDelayPerUnit(0.50);
    AnimationCache::getInstance()->addAnimation(stumpStand, "stumpStand");
    auto stumpMove = Animation::create();
    stumpMove->addSpriteFrame(
        SpriteFrameCache::getInstance()->getSpriteFrameByName("stump_move_1.png"));
    stumpMove->addSpriteFrame(
        SpriteFrameCache::getInstance()->getSpriteFrameByName("stump_move_2.png"));
    stumpMove->addSpriteFrame(
        SpriteFrameCache::getInstance()->getSpriteFrameByName("stump_move_3.png"));
    stumpMove->addSpriteFrame(
        SpriteFrameCache::getInstance()->getSpriteFrameByName("stump_move_4.png"));
    stumpMove->setDelayPerUnit(0.20);
    AnimationCache::getInstance()->addAnimation(stumpMove, "stumpMove");
    auto stumpHit = Animation::create();
    stumpHit->addSpriteFrame(
        SpriteFrameCache::getInstance()->getSpriteFrameByName("stump_hit.png"));
    stumpHit->setDelayPerUnit(0.60);
    AnimationCache::getInstance()->addAnimation(stumpHit, "stumpHit");
    auto stumpDown = Animation::create();
    stumpDown->addSpriteFrame(
        SpriteFrameCache::getInstance()->getSpriteFrameByName("stump_down_1.png"));
    stumpDown->addSpriteFrame(
        SpriteFrameCache::getInstance()->getSpriteFrameByName("stump_down_2.png"));
    stumpDown->addSpriteFrame(
        SpriteFrameCache::getInstance()->getSpriteFrameByName("stump_down_3.png"));
    stumpDown->setDelayPerUnit(0.10);
    AnimationCache::getInstance()->addAnimation(stumpDown, "stumpDown");
}

void
GameplayScene::initCharacter()
{
    TMXObjectGroup* temp = _map->getObjectGroup("player");
    auto ts = temp->getObject("birthPoint");

    float x = ts["x"].asFloat();
    float y = ts["y"].asFloat();

    auto characterTagList = GameData::getInstance()->getOnStageCharacterTagList();
    p1Player = Player::create(characterTagList[0]);
    p2Player = Player::create(characterTagList[1]);
    p1Player->setPosition(x, y);
    p2Player->setPosition(x, y);
    p1Player->retain();
    p2Player->retain();

    curPlayer = p1Player;
    curPlayer->setName("curPlayer");
    mapLayer->addChild(p1Player, MAP_LAYER_CHARACTER_ZORDER);

    curPlayer->changeAttackType(p1Player->currentAttackType);
}

void
GameplayScene::initCtrlPanel()
{
    controlPanel = CtrlPanelLayer::create();

    this->addChild(controlPanel, CTRLPANEL_LAYER_ZORDER);
}

void
GameplayScene::initArea()
{
    auto playerPos = curPlayer->getPosition();
    auto areas = _map->getObjectGroup("area")->getObjects();

    for (auto& v : areas) {
        auto dict = v.asValueMap();
        if (dict.size() == 0)
            continue;
        float x = dict["x"].asFloat();
        float y = dict["y"].asFloat();
        float width = dict["width"].asFloat();
        float height = dict["height"].asFloat();
        curArea.setRect(x, y, width, height);
        if (curArea.containsPoint(playerPos)) {
            //替换背景图片
            backgroundParallaxPicture->setTexture(dict["background"].asString());
            auto width = backgroundParallaxPicture->getContentSize().width;
            auto height = backgroundParallaxPicture->getContentSize().height;
            auto bigger = width > height ? width : height;
            float scale = visibleSize.width / height;
            backgroundParallaxPicture->setScale(scale * 1.1);

            //加载装饰物
            initDecoration(backgroundParallaxDecoration, "backgroundParallaxDecoration");
            initDecoration(backgroundStaticDecoration, "backgroundStaticDecoration");
            initDecoration(foregroundParallaxDecoration, "foregroundParallaxDecoration");
            initDecoration(foregroundStaticDecoration, "foregroundStaticDecoration");

            //替换背景音乐
            if (dict["bgm"].asString() == "") {
                AudioController::getInstance()->clearMusic();
            } else {
                AudioController::getInstance()->playMusic(dict["bgm"].asString(), true);
            }
            // 加载发射器
            initLauncher();
            // 加载电梯
            initElevator();
            // 加载敌人
            initEnemy();
            // 加载事件
            initEvent();
            break;
        }
    }
}

void
GameplayScene::initDecoration(Layer* layer, const std::string& objectGroup)
{
    //背景 视差
    layer->removeAllChildren();
    auto decorations = _map->getObjectGroup(objectGroup)->getObjects();
    for (auto& v : decorations) {
        auto decoration = v.asValueMap();
        if (decoration.size() == 0) {
            continue;
        }
        float x = decoration["x"].asFloat();
        float y = decoration["y"].asFloat();
        if (curArea.containsPoint(Point(x, y))) {
            auto newDecoration = Sprite::create(decoration["name"].asString());
            newDecoration->setAnchorPoint(Vec2::ANCHOR_MIDDLE);
            layer->addChild(newDecoration);
            auto offsetX = x - curArea.getMinX();
            auto offsetY = y - curArea.getMinY();
            newDecoration->setPosition(offsetX, offsetY);
        }
    }
}

void
GameplayScene::initCamera()
{
    camera = Node::create();
    mapLayer->addChild(camera, MAP_LAYER_CAMERA_ZORDER);

    Vec2 poi = curPlayer->getPosition();
    camera->setPosition(poi.x + 100, poi.y + 70); //移动摄像机

    auto cameraFollow = Follow::create(camera, curArea);
    cameraFollow->setTag(cameraTag);
    mapLayer->runAction(cameraFollow);
}

void
GameplayScene::initLauncher()
{
    TMXObjectGroup* group = _map->getObjectGroup("launcher");
    auto objects = group->getObjects();

    for (auto v : objects) {
        auto dict = v.asValueMap();
        if (dict.size() == 0)
            continue;
        float x = dict["x"].asFloat();
        float y = dict["y"].asFloat();

        if (curArea.containsPoint(Vec2(x, y))) {
            auto _launcher = Sprite::create("CloseNormal.png");
            _launcher->setPosition(x, y);
            mapLayer->addChild(_launcher, MAP_LAYER_OTHER_ZORDER);

            auto fe = Emitter::create((Node**)(&curPlayer));
            _launcher->addChild(fe);
            fe->playStyle(StyleType::SCATTER);
            // fe->playStyle(StyleType::ODDEVEN);

            launcherList.pushBack(_launcher);
        }
    }
}

void
GameplayScene::initElevator()
{
    TMXObjectGroup* group = _map->getObjectGroup("elevator");
    auto objects = group->getObjects();

    for (auto v : objects) {
        auto dict = v.asValueMap();
        if (dict.size() == 0)
            continue;
        //读取起始点
        float x = dict["x"].asFloat();
        float y = dict["y"].asFloat();

        if (curArea.containsPoint(Vec2(x, y))) {
            auto polyline_points = dict["polylinePoints"].asValueVector();
            vector<Vec2> vertex;
            for (auto obj : polyline_points) {
                float offx = obj.asValueMap()["x"].asFloat();
                float offy = obj.asValueMap()["y"].asFloat();
                vertex.push_back(Vec2((x + offx), (y - offy)));
            }
            auto seq = Sequence::create(DelayTime::create(1.0f), nullptr);

            for (auto obj : vertex) {
                seq = Sequence::createWithTwoActions(seq, MoveTo::create(1.0f, obj));
            }

            // Sequence::reverse()方法不支持moveTo
            auto seq_reverse = Sequence::create(DelayTime::create(1.0f), nullptr);
            vector<Vec2>::reverse_iterator it = vertex.rbegin();
            while (it != vertex.rend()) {
                seq_reverse =
                    Sequence::createWithTwoActions(seq_reverse, MoveTo::create(1.0f, *it));
                ++it;
            }

            auto _elevator = Elevator::create();
            _elevator->setPosition(x, y);
            mapLayer->addChild(_elevator, MAP_LAYER_OTHER_ZORDER);
            _elevator->runAction(
                RepeatForever::create(Sequence::create(seq, seq_reverse, nullptr)));

            elevatorList.pushBack(_elevator);
        }
    }
}

void
GameplayScene::initEnemy()
{
    TMXObjectGroup* group = _map->getObjectGroup("enemy");
    auto objects = group->getObjects();

    _bosses = 0;

    for (auto v : objects) {
        auto dict = v.asValueMap();
        if (dict.size() == 0)
            continue;

        float x = dict["x"].asFloat();
        float y = dict["y"].asFloat();

        if (curArea.containsPoint(Vec2(x, y))) {
            std::string tag = dict["tag"].asString();
            Enemy* _enemy = Enemy::create(tag);
            _enemy->setPosition(x, y);
            mapLayer->addChild(_enemy, MAP_LAYER_ENEMY_ZORDER);

            /*临时项*/
            _enemy->setTarget(curPlayer);
            _enemy->setEmitter();
            /*临时项*/

            enemyList.pushBack(_enemy);

            if (dict["type"].asString() == "boss") {
                _bosses++;
                auto ctrlLayer = (CtrlPanelLayer*)controlPanel;
                ctrlLayer->createBossHpBar(_enemy, _enemy->CurrentHp, _enemy->face);
            }
        }
    }
}

void
GameplayScene::initEvent()
{
    TMXObjectGroup* group = _map->getObjectGroup("event");
    auto objects = group->getObjects();
    for (auto v : objects) {
        auto dict = v.asValueMap();
        if (dict.size() == 0)
            continue;

        float x = dict["x"].asFloat();
        float y = dict["y"].asFloat();

        if (curArea.containsPoint(Vec2(x, y))) {
            std::string tag = dict["tag"].asString();
            auto _event = Sprite::create("gameplayscene/unknownEvent.png");
            _event->setPosition(x, y);
            _event->setName(tag);
            _event->setTag(eventCategoryTag);

            auto body = PhysicsBody::createBox(Size(15, 25));
            body->setGravityEnable(false);
            body->setRotationEnable(false);
            body->setCategoryBitmask(eventCategory);
            body->setCollisionBitmask(0);
            body->setContactTestBitmask(playerCategory);
            _event->setPhysicsBody(body);
            mapLayer->addChild(_event, MAP_LAYER_OTHER_ZORDER);

            eventPoint.pushBack(_event);
        }
    }
}

bool
GameplayScene::contactBegin(const PhysicsContact& contact)
{
    auto shapeA = contact.getShapeA();
    auto shapeB = contact.getShapeB();
    auto nodeA = contact.getShapeA()->getBody()->getNode();
    auto nodeB = contact.getShapeB()->getBody()->getNode();
    if (nodeA && nodeB) {
        auto tagA = nodeA->getTag();
        auto tagB = nodeB->getTag();
        // entityA和entityB对nodeA和nodeB进行映射
        Node* entityA;
        Node* entityB;
        PhysicsShape* entityA_shape;
        PhysicsShape* entityB_shape;

        // enemy相关
        if (tagA == enemyCategoryTag || tagB == enemyCategoryTag) {
            if (tagA == enemyCategoryTag) {
                entityA = nodeA;
                entityB = nodeB;
                entityA_shape = shapeA;
                entityB_shape = shapeB;
            } else if (tagB == enemyCategoryTag) {
                entityA = nodeB;
                entityB = nodeA;
                entityA_shape = shapeB;
                entityB_shape = shapeA;
            }

            // 当enemy碰到了折线刚体
            if (entityB->getTag() == polylineCategoryTag) {
                //当冲量方向向上时可以穿过折线刚体
                if (contact.getContactData()->normal.y > 0) {
                    auto _enemy = (Enemy*)entityA;
                    _enemy->resetJump();
                    return true;
                } else {
                    return false;
                }
            }
            // 当enemy碰到了地面刚体
            else if (entityB->getTag() == groundCategoryTag) {
                //什么也不做
            }
            // 当enemy碰到了子弹
            else if (entityB->getTag() == bulletCategoryTag) {
                ParticleSystem* _ps = ParticleExplosion::createWithTotalParticles(5);
                _ps->setTexture(Director::getInstance()->getTextureCache()->addImage(
                    "gameplayscene/smallOrb000.png"));

                // cocos2dx的粒子系统有三种位置类型
                mapLayer->addChild(_ps, MAP_LAYER_OTHER_ZORDER);
                _ps->setPositionType(ParticleSystem::PositionType::RELATIVE);
                _ps->setPosition(entityA->getPosition());
                _ps->setLife(1.2);
                _ps->setLifeVar(0.3);
                _ps->setEndSize(0.0f);
                _ps->setAutoRemoveOnFinish(true);

                auto _enemy = (Enemy*)entityA;
                auto _bullet = (Bullet*)entityB;

                DamageInfo _damageInfo;
                _damageInfo.damage = _bullet->getDamage();
                _damageInfo.target = _enemy;
                EventCustom event("bullet_hit_enemy");
                event.setUserData((void*)&_damageInfo);
                _eventDispatcher->dispatchEvent(&event);

                entityB->removeFromParentAndCleanup(true); //移除子弹
            }
            // 当enemy站在电梯上
            else if (entityB->getTag() == elevatorCategoryTag) {
                if (contact.getContactData()->normal.y > 0) {
                    auto _enemy = (Enemy*)entityA;
                    auto _elevator = (Elevator*)entityB;
                    _enemy->resetJump();
                    _elevator->addPassenger(_enemy);
                    return true;
                } else {
                    return false;
                }
            }

            //其他
        }
        // player相关
        if (tagA == playerCategoryTag || tagB == playerCategoryTag) {
            if (nodeA->getTag() == playerCategoryTag) {
                entityA = nodeA;
                entityB = nodeB;
                entityA_shape = shapeA;
                entityB_shape = shapeB;
            } else if (nodeB->getTag() == playerCategoryTag) {
                entityA = nodeB;
                entityB = nodeA;
                entityA_shape = shapeB;
                entityB_shape = shapeA;
            }

            //当player碰到了折线刚体
            if (entityB->getTag() == polylineCategoryTag) {
                if (contact.getContactData()->normal.y > 0) {
                    auto _player = (Player*)entityA;
                    _player->resetJump();
                    return true;
                } else {
                    return false;
                }
            }
            // 当player碰到了地面刚体
            else if (entityB->getTag() == groundCategoryTag) {
                // do nothing
            }
            // 当player碰到了敌人或索敌框
            else if (entityB->getTag() == enemyCategoryTag) {
                // 当player碰到了敌人的索敌框
                if (entityB_shape->getTag() == lockCategoryTag) {
                    auto _enemy = (Enemy*)entityB;
                    _enemy->stateMachine->autoChangeState();
                }
                // 当player碰到了敌人本身
                else {
                    DamageInfo _damageInfo;
                    _damageInfo.damage = 10;      //碰撞伤害
                    _damageInfo.target = entityA; //受损目标
                    EventCustom event("bullet_hit_player");
                    event.setUserData((void*)&_damageInfo);
                    _eventDispatcher->dispatchEvent(&event);
                }

            }
            // 当player碰到了事件点或者宝箱
            else if (entityB->getTag() == eventCategoryTag) {
                //抛出event的tag
                EventCustom event("trigger_event");
                event.setUserData((void*)entityB->getName().c_str());
                _eventDispatcher->dispatchEvent(&event);
                entityB->removeFromParent();
            }
            // 当player站在电梯上
            else if (entityB->getTag() == elevatorCategoryTag) {

                if (contact.getContactData()->normal.y > 0) {
                    auto _player = (Player*)entityA;
                    auto _elevator = (Elevator*)entityB;
                    _player->resetJump();
                    _elevator->addPassenger(curPlayer);
                    return true;
                } else {
                    return false;
                }
            }
            //其他
        }

        //飞行扫把
        if (tagA == elevatorCategoryTag || tagB == elevatorCategoryTag) {
            if (nodeA->getTag() == elevatorCategoryTag) {
                entityA = nodeA;
                entityB = nodeB;
                entityA_shape = shapeA;
                entityB_shape = shapeB;
            } else if (nodeB->getTag() == elevatorCategoryTag) {
                entityA = nodeB;
                entityB = nodeA;
                entityA_shape = shapeB;
                entityB_shape = shapeA;
            }

            //当拖把碰到了地形
            if (entityB->getTag() == groundCategoryTag) {
                entityA->removeFromParent();
            }
        }

        //其他
    }
    return true;
}

bool
GameplayScene::contactSeparate(const PhysicsContact& contact)
{
    auto nodeA = contact.getShapeA()->getBody()->getNode();
    auto nodeB = contact.getShapeB()->getBody()->getNode();
    if (nodeA && nodeB) {
        auto tagA = nodeA->getTag();
        auto tagB = nodeB->getTag();
        Node* entityA;
        Node* entityB;

        // enemy相关
        if (tagA == enemyCategoryTag || tagB == enemyCategoryTag) {
            if (tagA == enemyCategoryTag) {
                entityA = nodeA;
                entityB = nodeB;
            } else if (tagB == enemyCategoryTag) {
                entityA = nodeB;
                entityB = nodeA;
            }

            if (entityB->getTag() == elevatorCategoryTag) {
                auto _enemy = (Enemy*)entityA;
                auto _elevator = (Elevator*)entityB;
                _elevator->removePassenger(_enemy);
            }
            //其他
        }
        // player相关
        if (tagA == playerCategoryTag || tagB == playerCategoryTag) {
            if (nodeA->getTag() == playerCategoryTag) {
                entityA = nodeA;
                entityB = nodeB;
            } else if (nodeB->getTag() == playerCategoryTag) {
                entityA = nodeB;
                entityB = nodeA;
            }

            if (entityB->getTag() == elevatorCategoryTag) {
                auto _elevator = (Elevator*)entityB;
                _elevator->removePassenger(curPlayer);
            }
            //其他
        }
    }
    return true;
}

//初始化监听器，手动指定优先级
void
GameplayScene::initPhysicsContactListener()
{
    auto filter = EventListenerPhysicsContact::create();
    filter->onContactBegin = CC_CALLBACK_1(GameplayScene::contactBegin, this);
    filter->onContactSeparate = CC_CALLBACK_1(GameplayScene::contactSeparate, this);
    _eventDispatcher->addEventListenerWithFixedPriority(filter, 50);
}

void
GameplayScene::initCustomEventListener()
{
    _eventDispatcher->addCustomEventListener(
        "left_key_pressed", [this](EventCustom* e) { this->onEventLeftKeyPressed(e); });

    _eventDispatcher->addCustomEventListener(
        "right_key_pressed", [this](EventCustom* e) { this->onEventRightKeyPressed(e); });

    _eventDispatcher->addCustomEventListener(
        "motion_key_released", [this](EventCustom* e) { this->onEventMotionKeyReleased(e); });

    _eventDispatcher->addCustomEventListener(
        "jump_key_pressed", [this](EventCustom* e) { this->onEventJumpKeyPressed(e); });

    _eventDispatcher->addCustomEventListener(
        "dash_key_pressed", [this](EventCustom* e) { this->onEventDashKeyPressed(e); });

    _eventDispatcher->addCustomEventListener(
        "switch_character", [this](EventCustom* e) { this->onEventSwitchCharacter(e); });

    _eventDispatcher->addCustomEventListener(
        "switch_attack_type", [this](EventCustom* e) { this->onEventSwitchAttackType(e); });

    _eventDispatcher->addCustomEventListener(
        "settings_key_pressed", [this](EventCustom* e) { this->onEventSettingsKeyPressed(e); });

    _eventDispatcher->addCustomEventListener("bullet_hit_enemy", [this](EventCustom* e) {
        auto _damageInfo = (DamageInfo*)e->getUserData();
        auto _enemy = (Enemy*)_damageInfo->target;
        _enemy->decreaseHp(_damageInfo->damage);
    });

    _eventDispatcher->addCustomEventListener("bullet_hit_player", [this](EventCustom* e) {
        auto _damageInfo = (DamageInfo*)e->getUserData();
        auto _player = (Player*)_damageInfo->target;
        _player->getHit(_damageInfo, _eventFilterMgr);
    });

    _eventDispatcher->addCustomEventListener("use_item", [this](EventCustom* e) {
        string itemTag = (char*)e->getUserData();

        if (itemTag == "I1") {
            if (curPlayer->currentMana != curPlayer->baseMana) {
                Hp_Mp_Change mpChange;
                mpChange.tag = curPlayer->playerTag;
                mpChange.value = 30;

                EventCustom event("mp_change");
                event.setUserData((void*)&mpChange);
                _eventDispatcher->dispatchEvent(&event);

                curPlayer->currentMana += mpChange.value;
                if (curPlayer->currentMana > curPlayer->baseMana) {
                    curPlayer->currentMana = curPlayer->baseMana;
                }
            }
        } else if (itemTag == "I2") {
            if (curPlayer->currentMana != curPlayer->baseMana) {
                Hp_Mp_Change mpChange;
                mpChange.tag = curPlayer->playerTag;
                mpChange.value = 50;

                EventCustom event("mp_change");
                event.setUserData((void*)&mpChange);
                _eventDispatcher->dispatchEvent(&event);

                curPlayer->currentMana += mpChange.value;
                if (curPlayer->currentMana > curPlayer->baseMana) {
                    curPlayer->currentMana = curPlayer->baseMana;
                }
            }
        } else if (itemTag == "I3") {
            if (curPlayer->currentHP != curPlayer->baseHP) {
                Hp_Mp_Change hpChange;
                hpChange.tag = curPlayer->playerTag;
                hpChange.value = 100;

                EventCustom event("hp_change");
                event.setUserData((void*)&hpChange);
                _eventDispatcher->dispatchEvent(&event);

                curPlayer->currentHP += hpChange.value;
                if (curPlayer->currentHP > curPlayer->baseHP) {
                    curPlayer->currentHP = curPlayer->baseHP;
                }
            }
        } else if (itemTag == "I4") {
            if (curPlayer->currentHP != curPlayer->baseHP) {
                Hp_Mp_Change hpChange;
                hpChange.tag = curPlayer->playerTag;
                hpChange.value = 999;

                EventCustom event("hp_change");
                event.setUserData((void*)&hpChange);
                _eventDispatcher->dispatchEvent(&event);

                curPlayer->currentHP += hpChange.value;
                if (curPlayer->currentHP > curPlayer->baseHP) {
                    curPlayer->currentHP = curPlayer->baseHP;
                }
            }
        } else if (itemTag == "I7") {
            auto playerPos = curPlayer->getPosition();
            auto mop = Broom::create();
            mapLayer->addChild(mop);
            mop->setPosition(playerPos.x, playerPos.y + 50);
            std::function<void(Ref*)> remove = [mop](Ref*) { mop->removeFromParent(); };
            if (curPlayer->playerDirection == Direction::RIGHT) {
                mop->runAction(Sequence::create(DelayTime::create(1.0f),
                                                MoveBy::create(5.0, Vec2(1500, 0)),
                                                CallFuncN::create(remove), NULL));
            } else {
                mop->setScale(-1);
                mop->runAction(Sequence::create(DelayTime::create(1.0f),
                                                MoveBy::create(5.0, Vec2(-1500, 0)),
                                                CallFuncN::create(remove), NULL));
            }

        } else if (itemTag == "I8") {
            Vec2 impluse = Vec2(0.0f, 1500.0f);
            curPlayer->getPhysicsBody()->applyImpulse(impluse);
        }

        auto effect = Sprite::create();
        effect->setBlendFunc({ GL_SRC_ALPHA, GL_ONE });
        curPlayer->addChild(effect);
        auto animation = AnimationCache::getInstance()->getAnimation("use");
        std::function<void(Ref*)> remove = [effect](Ref*) { effect->removeFromParent(); };
        effect->runAction(
            Sequence::create(Animate::create(animation), CallFuncN::create(remove), NULL));

    });

    _eventDispatcher->addCustomEventListener("use_spell_card", [this](EventCustom* e) {
        string spellTag = (char*)e->getUserData();
        Hp_Mp_Change mpChange;
        if (spellTag == "C1") {
            mpChange.tag = curPlayer->playerTag;
            mpChange.value = -20;

            auto& sc = curPlayer->spellCardStyleConfig;
            sc.style = StyleType::PARABOLA;
            sc.frequency = 0.06f;
            sc.bulletDuration = 2.5;
            sc.number = 10;
            sc.countThenChangePos = 4;
            sc.cycleTimes = -1;
            sc.totalDuration = 0.8;
            sc.height = 30;
            sc.distance = 600;
            sc.startAngle = 10;
            sc.endAngle = 50;
            sc.bc.name = "b3_1_3.png";
            sc.bc.length = 25;
            sc.bc.width = 25;
            sc.bc.harm = 5;
            sc.bc._categoryBitmask = bulletCategory;
            sc.bc._collisionBitmask = enemyCategory;
            sc.bc._contactTestBitmask = enemyCategory;

            EventCustom event("mana_change");
            event.setUserData((void*)&mpChange);
            _eventDispatcher->dispatchEvent(&event);
            curPlayer->currentMana -= mpChange.value;
            if (curPlayer->currentMana < 0) {
                curPlayer->currentMana = 0;
            }

            this->curPlayer->stateMachine->changeState(Player::UseSpellCard::getInstance());
        } else if (spellTag == "C2") {
            mpChange.tag = curPlayer->playerTag;
            mpChange.value = -30;

            auto& sc = curPlayer->spellCardStyleConfig; //引用
            sc.style = StyleType::PARALLEL;

            sc.frequency = 0.2f;
            sc.bulletDuration = 2.4f;
            sc.countThenChangePos = 1;
            sc.interval = 1.2f;
            sc.number = 5;
            sc.cycleTimes = -1;
            sc.totalDuration = 2.0;

            sc.bc.name = "b3_2_1.png";
            sc.bc.length = 15;
            sc.bc.width = 15;
            sc.bc.harm = 5;
            sc.bc._categoryBitmask = bulletCategory;
            sc.bc._collisionBitmask = enemyCategory;
            sc.bc._contactTestBitmask = enemyCategory;

            EventCustom event("mana_change");
            event.setUserData((void*)&mpChange);
            _eventDispatcher->dispatchEvent(&event);
            curPlayer->currentMana -= mpChange.value;
            if (curPlayer->currentMana < 0) {
                curPlayer->currentMana = 0;
            }

            this->curPlayer->stateMachine->changeState(Player::UseSpellCard::getInstance());
        } else if (spellTag == "C3") {
            mpChange.tag = curPlayer->playerTag;
            mpChange.value = -30;

            auto& sc = curPlayer->spellCardStyleConfig; //引用

            sc.style = StyleType::SCATTER;

            sc.frequency = 0.10f;
            sc.bulletDuration = 3.0;
            sc.number = 5;
            sc.bc.name = "b1_3_3.png";
            sc.startAngle = 260;
            sc.endAngle = 280;
            sc.deltaAngle = 0;

            sc.cycleTimes = -1;
            sc.totalDuration = 2.0;

            sc.bc.name = "b3_2_1.png";
            sc.bc.length = 15;
            sc.bc.width = 15;
            sc.bc.harm = 5;
            sc.bc._categoryBitmask = bulletCategory;
            sc.bc._collisionBitmask = enemyCategory;
            sc.bc._contactTestBitmask = enemyCategory;

            EventCustom event("mana_change");
            event.setUserData((void*)&mpChange);
            _eventDispatcher->dispatchEvent(&event);
            curPlayer->currentMana -= mpChange.value;
            if (curPlayer->currentMana < 0) {
                curPlayer->currentMana = 0;
            }

            this->curPlayer->stateMachine->changeState(Player::UseSpellCard::getInstance());
        }

        AudioController::getInstance()->playEffect("se/use_spell_card.wav");
    });

    _eventDispatcher->addCustomEventListener("kill_boss", [this](EventCustom* e) {
        _bosses--;
        if (_bosses == 0) {
            auto ctrlLayer = (CtrlPanelLayer*)controlPanel;
            ctrlLayer->removeBossHpBar();

            auto sequence = Sequence::create(
                DelayTime::create(3.0f),
                CallFuncN::create(CC_CALLBACK_0(GameplayScene::endGame, this)), NULL);
            this->runAction(sequence);
        }
    });
}

void
GameplayScene::endGame()
{
    auto roundInformation = this->_map->getObjectGroup("RoundInformation");
    auto endEvent = roundInformation->getProperty("onExitTriggerEvent").asString();

    EventCustom event("trigger_event");
    event.setUserData((void*)endEvent.c_str());
    _eventDispatcher->dispatchEvent(&event);
}

void
GameplayScene::onEventLeftKeyPressed(EventCustom*)
{
    curPlayer->playerSprite->setScaleX(-1); //人物翻转
    curPlayer->playerDirection = Direction::LEFT;

    curPlayer->schedule(CC_SCHEDULE_SELECTOR(Player::horizontallyAccelerate));
}

void
GameplayScene::onEventRightKeyPressed(EventCustom*)
{
    curPlayer->playerSprite->setScaleX(1); //人物翻转
    curPlayer->playerDirection = Direction::RIGHT;

    curPlayer->schedule(CC_SCHEDULE_SELECTOR(Player::horizontallyAccelerate));
}

void
GameplayScene::onEventMotionKeyReleased(EventCustom*)
{
    if (curPlayer->isScheduled(CC_SCHEDULE_SELECTOR(Player::horizontallyAccelerate))) {
        curPlayer->unschedule(CC_SCHEDULE_SELECTOR(Player::horizontallyAccelerate));
    }

    if (curPlayer->stateMachine->getCurrentState() == Player::Walk::getInstance()) {
        //减速
        auto currentVelocity = curPlayer->getPhysicsBody()->getVelocity();
        curPlayer->getPhysicsBody()->setVelocity(Vec2(currentVelocity.x / 3.0f, currentVelocity.y));
    }
}

void
GameplayScene::onEventJumpKeyPressed(EventCustom*)
{
    if (curPlayer->jumpCounts == 0) {
        return;
    }
    if (curPlayer->stateMachine->getCurrentState() == Player::UseSpellCard::getInstance()) {
        return;
    }
    curPlayer->stateMachine->changeState(Player::Jump::getInstance());
}

void
GameplayScene::onEventDashKeyPressed(EventCustom*)
{
    if (curPlayer->dashCounts == 0) {
        return;
    }
    if (curPlayer->stateMachine->getCurrentState() == Player::UseSpellCard::getInstance()) {
        return;
    }
    curPlayer->stateMachine->changeState(Player::Dash::getInstance());
}

void
GameplayScene::onEventSwitchCharacter(EventCustom*)
{
    AudioController::getInstance()->playClickButtonEffect();

    Player* theOther = nullptr;

    if (curPlayer == p1Player) {
        theOther = p2Player;
    } else {
        theOther = p1Player;
    }

    curPlayer->stopAttackType();
    mapLayer->removeChild(curPlayer, false);

    if (curPlayer->playerDirection == Direction::RIGHT) {
        theOther->playerDirection = Direction::RIGHT;
        theOther->playerSprite->setScaleX(1);
    } else {
        theOther->playerDirection = Direction::LEFT;
        theOther->playerSprite->setScaleX(-1);
    }

    theOther->getPhysicsBody()->setVelocity(curPlayer->getPhysicsBody()->getVelocity());
    theOther->setPosition(curPlayer->getPosition());
    theOther->changeAttackType(theOther->currentAttackType);

    curPlayer = theOther;
    mapLayer->addChild(theOther, MAP_LAYER_CHARACTER_ZORDER);

    auto effect = Sprite::create("effect/comboLimit.png");
    BlendFunc cbl = { GL_SRC_ALPHA, GL_ONE };
    effect->setBlendFunc(cbl);
    effect->setOpacity(130);
    curPlayer->addChild(effect);
    std::function<void(Ref*)> remove = [effect](Ref*) { effect->removeFromParent(); };
    effect->runAction(Sequence::create(Repeat::create(RotateBy::create(1.0, 360), 2),
                                       CallFuncN::create(remove), NULL));
}

void
GameplayScene::onEventSwitchAttackType(EventCustom*)
{
    curPlayer->stopAttackType();
    if (curPlayer->currentAttackType == curPlayer->type1.tag) {
        curPlayer->changeAttackType(curPlayer->type2.tag);
    } else {
        curPlayer->changeAttackType(curPlayer->type1.tag);
    }
}

void
GameplayScene::onEventSettingsKeyPressed(EventCustom*)
{
    AudioController::getInstance()->playClickButtonEffect();

    auto layer = SettingsLayer::create("GameplayScene");
    layer->setPauseNode(mapLayer);
    mapLayer->onExit();
    this->addChild(layer, SETTING_LAYER_ZORDER);
}

void
GameplayScene::update(float dt)
{
    Vec2 poi = curPlayer->getPosition();
    camera->setPosition(poi.x + 100, poi.y + 70); //移动摄像机

    //如果地图切换区域后首次执行update函数，则首先进行以下初始化操作
    if (isParallaxPositionInit) {
        mapLayerPrePosition = mapLayer->getPosition();
        if (mapLayer->getPosition().x <= 0) {
            auto offsetX = abs(mapLayer->getPosition().x) - curArea.getMinX();
            auto offsetY = abs(mapLayer->getPosition().y) - curArea.getMinY();
            backgroundParallaxNode->setPosition(-offsetX, -offsetY);
            foregroundParallaxNode->setPosition(-offsetX, -offsetY);
        } else {
            //一般而言地图偏移量并不会为正
            auto offsetX = mapLayer->getPosition().x - curArea.getMinX();
            auto offsetY = mapLayer->getPosition().y - curArea.getMinY();
            backgroundParallaxNode->setPosition(offsetX, offsetY);
            foregroundParallaxNode->setPosition(offsetX, offsetY);
        }
        backParallaxNodePrePosition = backgroundParallaxNode->getPosition();
        foreParallaxNodePrePosition = foregroundParallaxNode->getPosition();
        isParallaxPositionInit = false;
    }

    //取得TMX地图层的偏移量
    auto mapLayerCurPosition = mapLayer->getPosition();
    auto offsetX = mapLayerCurPosition.x - mapLayerPrePosition.x;
    auto offsetY = mapLayerCurPosition.y - mapLayerPrePosition.y;
    mapLayerPrePosition = mapLayerCurPosition;
    //设置背景视差结点的偏移量
    backgroundParallaxNode->setPosition(backParallaxNodePrePosition.x + offsetX,
                                        backParallaxNodePrePosition.y + offsetY);
    backParallaxNodePrePosition =
        Vec2(backParallaxNodePrePosition.x + offsetX, backParallaxNodePrePosition.y + offsetY);
    //设置前景视差结点的偏移量
    foregroundParallaxNode->setPosition(foreParallaxNodePrePosition.x + offsetX,
                                        foreParallaxNodePrePosition.y + offsetY);
    foreParallaxNodePrePosition =
        Vec2(foreParallaxNodePrePosition.x + offsetX, foreParallaxNodePrePosition.y + offsetY);

    if (curArea.containsPoint(poi)) {
        ;
    } else {

        //移除前一个区域的物件
        for (auto v : enemyList) {
            v->removeFromParentAndCleanup(true);
        }
        for (auto v : eventPoint) {
            v->removeFromParentAndCleanup(true);
        }
        for (auto v : launcherList) {
            v->removeFromParentAndCleanup(true);
        }
        for (auto v : elevatorList) {
            v->removeFromParentAndCleanup(true);
        }
        if (_bosses != 0) {
            auto ctrlLayer = (CtrlPanelLayer*)controlPanel;
            ctrlLayer->removeBossHpBar();
        }

        initArea();

        //重置摄像机跟随
        mapLayer->stopActionByTag(cameraTag);
        auto cameraFollow = Follow::create(camera, curArea);
        cameraFollow->setTag(cameraTag);
        mapLayer->runAction(cameraFollow);

        //重置偏移量，重置视差节点位置
        isParallaxPositionInit = true;
    }
}
