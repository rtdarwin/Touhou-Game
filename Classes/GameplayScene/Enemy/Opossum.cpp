﻿#ifdef WIN32
#pragma execution_character_set("utf-8")
#endif

#include "GameplayScene/Enemy/Opossum.h"

bool
Opossum::init(std::string tag)
{
    if (!Node::init())
        return false;

    this->enemyTag = tag;
    this->setTag(enemyCategoryTag);
    this->setName(tag);

    enemySprite = Sprite::create(
        "gameplayscene/Enemy/opossum-1.png"); //此处必须初始化一张纹理，否则后面无法切换纹理
    this->addChild(enemySprite);

    //设置属性值
    EnemyData enemyData = GameData::getInstance()->getEnemyByTag(tag);
    this->hp = enemyData.healthPoint;

    //设置刚体
    body = PhysicsBody::createBox(
        enemySprite->getContentSize()); //这里要用包围盒，如果用圆形的话会导致滚动
    body->setDynamic(true);
    body->setMass(1);
    body->setGravityEnable(true);
    body->setRotationEnable(false);
    body->getFirstShape()->setDensity(0);
    body->getFirstShape()->setFriction(0.2);
    body->getFirstShape()->setRestitution(0); //设置刚体回弹力
    body->setCategoryBitmask(enemyCategory);
    body->setCollisionBitmask(groundCategory | bulletCategory);
    body->setContactTestBitmask(groundCategory | bulletCategory);
    this->setPhysicsBody(body);

    //索敌检测框
    auto rect = PhysicsShapeBox::create(Size(150, 200));
    rect->setCategoryBitmask(lockCategory);
    rect->setCollisionBitmask(0);
    rect->setContactTestBitmask(playerCategory);
    body->addShape(rect);

    //设置速度上限
    body->setVelocityLimit(500);

    //设置动画
    runAnimation = Animation::create();
    runAnimation->retain();
    for (int i = 1; i <= 6; i++)
        runAnimation->addSpriteFrameWithFile("gameplayscene/Enemy/opossum-" + std::to_string(i) +
                                             ".png");
    runAnimation->setDelayPerUnit(0.15f);
    AnimationCache::getInstance()->addAnimation(runAnimation, "OpossumRunAnimation");

    enemySprite->runAction(RepeatForever::create(Animate::create(runAnimation))); //初始时刻在奔跑

    //启动状态更新
    this->schedule(CC_SCHEDULE_SELECTOR(Opossum::autoSwitchAnimation), 0.1);

    return true;
}

void
Opossum::run(float dt)
{
    if (this->enemyDirection == Direction::LEFT) {

        this->getPhysicsBody()->applyImpulse(Vec2(-30.0f, 0.0f));
    } else {

        this->getPhysicsBody()->applyImpulse(Vec2(30.0f, 0.0f));
    }
}

void
Opossum::jump()
{
    return;
}

void
Opossum::decreaseHp(Node* node)
{
    auto enemy = (Enemy*)node;
    enemy->hp = enemy->hp - 10;
    if (enemy->hp < 0) {
        enemy->removeFromParentAndCleanup(true);
    }
}

void
Opossum::switchMode()
{
    if (this->isScheduled(CC_SCHEDULE_SELECTOR(Opossum::alertMode))) {
        this->unschedule(CC_SCHEDULE_SELECTOR(Opossum::alertMode));
    }
    if (this->isScheduled(CC_SCHEDULE_SELECTOR(Opossum::patrolMode))) {
        this->unschedule(CC_SCHEDULE_SELECTOR(Opossum::patrolMode));
    }

    if (this->curState == EnemyActionMode::Patrol) {
        this->schedule(CC_SCHEDULE_SELECTOR(Opossum::patrolMode), 1.0);
    } else if (this->curState == EnemyActionMode::Alert) {
        this->schedule(CC_SCHEDULE_SELECTOR(Opossum::alertMode), 0.50);
        this->schedule(CC_SCHEDULE_SELECTOR(Opossum::autoChangeDirection), 0.50);
    }
}

void
Opossum::alertMode(float dt)
{
    if (!(this->isScheduled(CC_SCHEDULE_SELECTOR(Opossum::run)))) {
        this->schedule(CC_SCHEDULE_SELECTOR(Opossum::run), 0.1);
    }
}

void
Opossum::patrolMode(float dt)
{
    int weight = random(0, 100);
    if (weight >= 60) {
        if (this->isScheduled(CC_SCHEDULE_SELECTOR(Opossum::run))) {
            this->unschedule(CC_SCHEDULE_SELECTOR(Opossum::run));
        }
    } else {
        if (weight >= 30) {
            enemySprite->setScaleX(-1);
            this->enemyDirection = Direction::RIGHT;
        } else {
            enemySprite->setScaleX(1);
            this->enemyDirection = Direction::LEFT;
        }

        if (!(this->isScheduled(CC_SCHEDULE_SELECTOR(Opossum::run)))) {
            this->schedule(CC_SCHEDULE_SELECTOR(Opossum::run), 0.1);
        }
    }
}

void
Opossum::autoChangeDirection(float dt)
{
    Point enemyPos = this->getPosition();
    Vec2 playerPos = (*curPlayer)->getPosition();
    if (enemyPos.x > playerPos.x) {
        this->enemyDirection = Direction::LEFT;
        enemySprite->setScaleX(1);
    } else {
        this->enemyDirection = Direction::RIGHT;
        enemySprite->setScaleX(-1);
    }
}

void
Opossum::autoSwitchAnimation(float dt)
{
    return;
}
