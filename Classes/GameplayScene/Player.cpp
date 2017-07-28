﻿#ifdef WIN32
#pragma execution_character_set("utf-8")
#endif

#include "GameplayScene/Player.h"
#include "GameData/GameData.h"
#include "GameplayScene/common.h"

bool
Player::init(std::string tag)
{
    if (!Node::init())
        return false;

    animateManager = AnimateManager::getInstance();
    //此处必须初始化一张角色纹理，否则后面无法切换纹理
    playerSprite = Sprite::create(animateManager->addTexture(tag));
    this->addChild(playerSprite);

    // 设置攻击方式
    vector<Character::Attack> selectAttackList =
        GameData::getInstance()->getSelectedAttackList(tag);
    type1 = selectAttackList[0];
    type2 = selectAttackList[1];
    currentAttackType = type1.tag;

    // 设置道具
    itemList = GameData::getInstance()->getCharacterItemList(tag);

    //设置属性值
    //留空

    //设置刚体
    auto body = PhysicsBody::createBox(Size(50, 75));
    body->setDynamic(true);
    body->setMass(1);
    body->setGravityEnable(true);
    body->setRotationEnable(false);
    body->getFirstShape()->setDensity(0);
    body->getFirstShape()->setFriction(0.2);
    body->getFirstShape()->setRestitution(0);
    body->setCategoryBitmask(playerCategory);
    body->setCollisionBitmask(groundCategory | enemyCategory);
    body->setContactTestBitmask(groundCategory | enemyCategory);
    this->setPhysicsBody(body);
    this->setTag(playerTag);
    this->setName(tag);

    //设置动画
    runAnimation = animateManager->addRunCache(tag);
    jumpAnimation = animateManager->addJumpCache(tag);
    dashAnimation = animateManager->addDashCache(tag);
    playerSprite->runAction(
        RepeatForever::create(Animate::create(runAnimation))); //初始时刻角色在奔跑

    bulletBatchNode =
        SpriteBatchNode::create("gameplayscene/bullet1.png"); //创建BatchNode节点，成批渲染子弹
    this->addChild(bulletBatchNode);

    return true;
}

void
Player::playerRun(float dt)
{
    auto body = this->getPhysicsBody();
    auto velocity = body->getVelocity();

    if (this->playerDirection.compare("right")) {
        Vec2 impluse = Vec2(0, 0);
        // Vec2 impluse = Vec2(-20.0f, 0.0f);
        // body->applyForce(Vec2(-100.0f, 0.0f));

        if (velocity.x > 10) {
            body->setVelocity(Vec2(-100, velocity.y));
        }

        if (velocity.x > -MAX_SPEED) {
            impluse.x = -std::min(MAX_SPEED / ACCELERATE_TIME * dt, MAX_SPEED + velocity.x);
        }
        body->applyImpulse(impluse);
    } else {
        Vec2 impluse = Vec2(0, 0);
        // Vec2 impluse = Vec2(20.0f, 0.0f);
        // body->applyForce(Vec2(100.0f,0.0f));

        if (velocity.x < -10) {
            body->setVelocity(Vec2(100, velocity.y));
        }

        if (velocity.x < MAX_SPEED) {
            impluse.x = std::min(MAX_SPEED / ACCELERATE_TIME * dt, MAX_SPEED - velocity.x);
        }
        body->applyImpulse(impluse);
    }
}

void
Player::playerJump()
{
    if (this->jumpCounts == 0) {
        return;
    }
    auto body = this->getPhysicsBody();
    auto velocity = body->getVelocity();
    body->setVelocity(Vec2(velocity.x, 0)); //再次跳跃时，重置Y轴速度为0

    //留空，在空中时不再接受水平加速，只有惯性
    //留空，对于不同的角色机制应有不同

    Vec2 impluse = Vec2(0.0f, 500.0f);
    body->applyImpulse(impluse);

    playerSprite->stopAllActions();
    Animate* animate = Animate::create(jumpAnimation);
    auto actionDone = CallFuncN::create(CC_CALLBACK_1(Player::resetAction, this));
    auto sequence = Sequence::create(Repeat::create(animate, 1), actionDone, NULL);
    playerSprite->runAction(sequence);

    this->jumpCounts--;
}

void
Player::playerDash()
{
    //留空，阻止连续dash，可增加角色状态标志

    if (this->dashCounts == 0) {
        return;
    }
    auto body = this->getPhysicsBody();
    auto velocity = body->getVelocity();
    body->setVelocity(Vec2(velocity.x, 0)); // dash时，重置Y轴速度为0

    //留空，将y轴速度短暂锁定为0，可以使角色不受重力
    //留空，对于不同的角色机制应有不同

    if (this->playerDirection.compare("right")) //当比对相等时返回false,wtf???
    {
        Vec2 impluse = Vec2(-350.0f, 0.0f);
        body->applyImpulse(impluse);
    } else {
        Vec2 impluse = Vec2(350.0f, 0.0f);
        body->applyImpulse(impluse);
    }

    playerSprite->stopAllActions();
    Animate* animate = Animate::create(dashAnimation);
    auto actionDone = CallFuncN::create(CC_CALLBACK_1(Player::resetAction, this));
    auto sequence = Sequence::create(Repeat::create(animate, 1), actionDone, NULL);
    playerSprite->runAction(sequence);

    this->dashCounts--;
}

void
Player::resetAction(Node* node)
{
    playerSprite->stopAllActions();
    playerSprite->runAction(RepeatForever::create(Animate::create(runAnimation)));
}

void
Player::regainDashCounts(float dt)
{
    this->dashCounts++;
}

void
Player::changeAttackType(const std::string& startType)
{
    if (startType == "reimu focus attack 1") {
        this->schedule(CC_SCHEDULE_SELECTOR(Player::ShootBullet), 0.5f);
    } else if (startType == "reimu focus attack 2") {

    } else if (startType == "reimu split attack 1") {

    } else if (startType == "reimu split attack 2") {

    } else if (startType == "marisa focus attack 1") {

    } else if (startType == "marisa focus attack 2") {

    } else if (startType == "marisa split attack 1") {

    } else if (startType == "marisa split attack 2") {
        this->schedule(CC_SCHEDULE_SELECTOR(Player::ShootBullet), 0.2f);
    }

    this->currentAttackType = startType;
}

void
Player::stopAttackType(const std::string& stopType)
{
    if (stopType == "reimu focus attack 1") {
        this->unschedule(CC_SCHEDULE_SELECTOR(Player::ShootBullet));
    } else if (stopType == "reimu focus attack 2") {

    } else if (stopType == "reimu split attack 1") {

    } else if (stopType == "reimu split attack 2") {

    } else if (stopType == "marisa focus attack 1") {

    } else if (stopType == "marisa focus attack 2") {

    } else if (stopType == "marisa split attack 1") {

    } else if (stopType == "marisa split attack 2") {
        this->unschedule(CC_SCHEDULE_SELECTOR(Player::ShootBullet));
    }
}

void
Player::useItem(const std::string& itemTag)
{
    if (itemTag == "I1") {
        this->playerJump();
    } else if (itemTag == "I2") {
        this->playerJump();
    } else if (itemTag == "I3") {

    } else if (itemTag == "I4") {
        this->playerJump();
    } else if (itemTag == "I5") {

    } else if (itemTag == "I6") {

    } else if (itemTag == "I7") {

    } else if (itemTag == "I8") {

    } else if (itemTag == "I9") {

    } else if (itemTag == "I10") {

    } else if (itemTag == "I11") {

    } else if (itemTag == "I12") {
    }
}

void
Player::useSpellCard(const std::string& cardTag)
{
    if (cardTag == "C1") {
        this->playerJump();
    } else if (cardTag == "C2") {
        this->playerDash();
    } else if (cardTag == "C3") {
        this->playerDash();
    } else if (cardTag == "C4") {
        this->playerJump();
    } else if (cardTag == "C5") {
        this->playerJump();
    }
}

//用缓存的方法创建子弹，并初始化子弹的运动和运动后的事件
void
Player::ShootBullet(float dt)
{
    Size winSize = Director::getInstance()->getWinSize();
    //从缓存中创建子弹
    auto spritebullet = Sprite::createWithTexture(bulletBatchNode->getTexture());
    spritebullet->setTag(bulletTag);

    //将创建好的子弹添加到BatchNode中进行批次渲染
    bulletBatchNode->addChild(spritebullet);

    //给创建好的子弹添加刚体
    do {
        auto _body = PhysicsBody::createBox(spritebullet->getContentSize());
        _body->setRotationEnable(false);
        _body->setGravityEnable(false);

        _body->setContactTestBitmask(bulletCategory);
        _body->setCollisionBitmask(enemyCategory);
        _body->setContactTestBitmask(enemyCategory);
        spritebullet->setPhysicsBody(_body);
    } while (0);

    //将创建好的子弹添加到容器
    vecBullet.pushBack(spritebullet);

    float realFlyDuration = 1.0;
    //子弹运行的距离和时间
    auto actionMove = MoveBy::create(realFlyDuration, Point(winSize.width, 0));
    auto fire1 = actionMove;

    if (this->playerDirection.compare("right")) {
        fire1 = actionMove->reverse();
    }

    //子弹执行完动作后进行函数回调，调用移除子弹函数
    auto actionDone = CallFuncN::create(CC_CALLBACK_1(Player::removeBullet, this));

    //子弹开始跑动
    Sequence* sequence = Sequence::create(fire1, actionDone, NULL);

    spritebullet->runAction(sequence);
}

//移除子弹，将子弹从容器中移除，同时也从SpriteBatchNode中移除
void
Player::removeBullet(Node* pNode)
{
    if (NULL == pNode) {
        return;
    }
    Sprite* bullet = (Sprite*)pNode;
    this->bulletBatchNode->removeChild(bullet, true);
    vecBullet.eraseObject(bullet);
}