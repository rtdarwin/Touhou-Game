#ifndef GAME_DATA_H
#define GAME_DATA_H

#include "cocos2d.h"
#include <string>
#include <utility>
#include <vector>

// 前向声明无法声明全称限定的类型名（C++ 的坑）
// 所以不再使用前向声明，直接包含头文件
#include "GameData/Character.h"
#include "GameData/Conversation.h"
#include "GameData/Item.h"
#include "GameData/Location.h"
#include "GameData/Round.h"
#include "GameData/Save.h"
#include "GameData/SpellCard.h"

USING_NS_CC;
using namespace std;

class GameData
{
public:
    static GameData* getInstance();

    ////////////////////////////////////////////////////////////////////////////////
    // 存档读取、删除、切换 Save

    int getCurrentSaveTag();
    vector<Save> getSaveList();
    void saveSave();
    void deleteSave(int saveTag);
    void switchSave(int saveTag);

    ////////////////////////////////////////////////////////////////////////////////
    // 对话 Conversation

    // 调整对话显示速度，1 为标准速度，1 以下为减缓，1 以上为加快，与 1 的比例为速度相对比
    float getConversationSpeed();
    void setConversationSpeed(float relativeSpeed);

    // 返回 vector.empty() = true 时表示这个地点中无对话
    // 在 HomeScene 中使用
    vector<ConversationIndicator> getConversationIndicatorList(const string& locationTag);

    // 通过对话编号返回详细对话内容，包括对话的双方，对话的内容，对话过程中人物的变化
    // 在 ConversationLayer 中使用
    vector<Dialogue> getConversation(const string& conversationTag);

    ////////////////////////////////////////////////////////////////////////////////
    // 地点关卡与进度 Location / Round / Process
    // 区分 Location / Map / Round , Location 是地点，Map 特指 TMX 地图，Round 则是一个个关卡

    // 获取当前所处地点的详情
    // 在 HomeScene RoundSelectScene 中使用
    Location getCurrentLocation();

    // 只返回已解锁地点的信息
    // 在 LocationSelectScene 中使用
    vector<Location> getLocationList();

    vector<string> getUnlockedLocationTagList();

    bool switchLocation(const string& newLocationTag);

    // 返回这个地点包含的关卡信息
    // 在 RoundSelectScene 中使用
    vector<Round> getRoundList(const string& locationTag);

    ////////////////////////////////////////////////////////////////////////////////
    // 人物属性与钱币 Character / Money

    // 返回现在可使用的角色的列表
    vector<Character> getAvailableCharacterList();

    // 返回选定的要出战的角色的列表
    // 在 RoundSelectScene 中使用
    vector<Character> getOnStageCharacterTagList();

    // 获得角色的列表后，传入角色标签，自动切换为队伍其他角色
    // 在 RoundSelectScene 中使用
    bool switchOnStageCharacter(int nth, const string& characterTag);

    // 获得指定角色使用或装备的道具列表
    vector<Item> getCharacterItemList(const string& characterTag);

    // 获得指定的道具列表后，更换角色装备的道具
    bool changeItem(const string& characterTag, int slot, const string& itemTag);

    // 获得指定角色的符卡列表，包括专属符卡和购买的通用符卡
    vector<SpellCard> getCharacterSpellCardList(const string& characterTag);

    // 获得角色的符卡列表后，传入角色标签，符卡标签和栏位，更换该栏位的符卡
    bool changeSpellCard(const string& characterTag, int slot, const string& spellCardTag);

    // 获得指定角色的攻击方式列表，包括集中和发散两种攻击方式
    vector<Character::Attack> getAttackList(const string& characterTag);

    // 获得角色的攻击方式列表后，传入角色标签，更换的攻击方式类型，自动切换为下一个攻击方式
    bool switchAttackType(const string& characterTag, Character::Attack::Type type);

    // int 类型可能不足以存储钱币数量，故使用 long 类型
    long getMoneyNum();

    // num 可为正可为负，为正时增加钱币数，为负时减少钱币数
    void increaseMoney(long num);

    ////////////////////////////////////////////////////////////////////////////////
    // 道具与符卡与符咒 Item / Card / Spell

    // pair.first 是符卡， pair.second 是符卡数量
    vector<pair<Item, int>> getAvailableItemList();
    vector<Item> getItemListInStore(const string& storeTag);
    vector<pair<SpellCard, int>> getAvailableSpellCardList();
    void buyItem(const string& itemTag);

    ////////////////////////////////////////////////////////////////////////////////
    // 对话/通关奖励 Award

    void ConversationComplete(const string& conversationTag);
    void RoundComplete(const string& roundTag);

    ////////////////////////////////////////////////////////////////////////////////
    // 资料库

    // 待定

private:
    GameData();
    bool init();

private:
    static GameData* _self;
};

#endif