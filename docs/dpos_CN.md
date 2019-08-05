# dpos接口详细

[English](dpos.md) | 中文

<!-- TOC -->

- [DPOS合约系统](#DPOS合约系统)
  - [创建选举合约](#创建选举合约)
  - [选举合约升级](#选举合约升级)
  - [选举配置](#选举配置)
  - [用户角色](#用户角色)
  - [提案动作](#提案动作)
  - [用户接口](#用户接口)
  - [角色权职](#角色权职)
  - [操作反馈](#操作反馈)
- [共识节点选举](#共识节点选举)
  - [申请成为候选共识节点](#申请成为候选共识节点)
  - [追加质押金](#追加质押金)
  - [切换节点地址](#切换节点地址)
  - [设置投票奖励分配](#设置投票奖励分配)
  - [为候选共识节点投票](#为候选共识节点投票)
  - [撤销投票](#撤销投票)
  - [候选共识节点退出](#候选共识节点退出)
  - [废止恶意共识节点提案](#废止恶意共识节点提案)
  - [共识节点查询](#共识节点查询)
    - [查询当前共识节点集合](#查询当前共识节点集合)
    - [查询候选共识节点集合信息](#查询候选共识节点集合信息)
    - [查询用户的共识节点投票信息](#查询用户的共识节点投票信息)
    - [查询共识节点申请信息](#查询共识节点申请信息)
    - [查询指定的废止恶意节点提案的信息](#查询指定的废止恶意节点提案的信息)
- [委员会](#委员会)
  - [委员会成员初始化设置](#委员会成员初始化设置)
  - [委员会选举](#委员会选举)
    - [申请加入委员会](#申请加入委员会)
    - [委员会批准投票](#委员会批准投票)
    - [委员会成员退出](#委员会成员退出)
    - [委员会查询](#委员会查询)
  - [选举配置更新](#共识节点选举配置更新)
    - [选举配置结构](#选举配置结构)
    - [选举配置更新提案](#选举配置更新提案)
    - [查询选举配置信息](#查询选举配置信息)
- [社区激励](#社区激励)
  - [生态节点申请](#生态节点申请)
  - [追加质押金](#追加质押金)
  - [设置投票奖励分配](#设置投票奖励分配)
  - [生态节点退出](#生态节点退出)
  - [生态节点投票和撤销投票](#生态节点投票和撤销投票)
  - [查询生态节点申请信息](#查询生态节点申请信息)
  - [查询用户的生态节点投票信息](#查询用户的生态节点投票信息)
  - [查询当前生态节点集合信息](#查询当前生态节点集合信息)
  - [查询候选生态节点集合信息](#查询候选生态节点集合信息)
  - [查询节点奖励信息](#查询节点奖励信息)
- [其他接口](#其他接口)
  - [奖励提取](#奖励提取)
  - [清理过期提案](#清理过期提案)

<!-- /TOC -->

## DPOS合约系统

### 创建选举合约

dpos 合约账户创建成功后，才可以进行后续的操作，且该账户是全局唯一的, 不能重复创建。

### 选举合约升级

- 由于 dpos 合约已经存在于区块链系统中，而合约创建后无法更改，所以需要通过版本升级的方式更新。Bumo v1.2.0 之后的版本自动使用新合约地址，老合约地址(buQtxgoaDrVJGtoPT66YnA2S84yE8FbBqQDJ)将废弃不用。
- 为方便dpos合约后续升级，避免每次都借助版本升级功能，新版 dpos 合约以 delegateCall 机制实现。入口合约使用delegateCall将调用委托给逻辑合约执行，delegateCall 可以指定逻辑合约的地址。所以，合约升级时，只需要创建一个新的逻辑合约，然后将入口合约中存储的逻辑合约地址更改为新地址即可，是否更新由 [委员会](#委员会) 投票决定。
- 由于合约创建时需要进行调用测试，所以，创建逻辑合约后，才能创建入口合约。
- 创建 dpos 逻辑合约时无需指定合约地址，由系统自动生成。创建 dpos 入口合约时则需要指定合约地址为：buQqzdS9YSnokDjvzg4YaNatcFQfkgXqk6ss，并且在入口合约中指定已经创建好的逻辑合约的地址。

#### 创建逻辑合约

创建逻辑合约账户时，将 src\contract\dpos.js 文件中的源码全部拷贝作为账户中 payload 字段的值。

>例

```json
  "contract" :
  {
    "payload" : "拷贝 src\contract\dpos.js 中全部代码至此处"
  },
```

#### 创建入口合约

创建入口合约时，先获取到逻辑合约的地址，填入创建合约时init_input参数中的logical_contract字段，然后将初始委员会成员地址列表填入committee字段，再将 dpos_delegate.js 文件中的源码全部拷贝作为账户中的 payload 字段的值。

创建示例

```json
  "create_account":
  {
    "dest_address": "buQqzdS9YSnokDjvzg4YaNatcFQfkgXqk6ss",
    "contract" :
    {
      "payload" : "拷贝 src\contract\dpos_delegate.js 中全部代码至此处"
    },
    "init_balance": 1000000,
    "init_input" : "{
      \"method\": \"init\",
      \"params\": {
        \"logic_contract\": \"此处填入逻辑合约地址\",
        \"committee\": [\"此处填入委员会地址列表\"]
      }
    }",
    "priv": {
      "master_weight": 0,
      "thresholds": {
        "tx_threshold": 1
      }
    }
  }
  
```

### 选举配置

 在 dpos.js 的合约代码中, 以下配置可根据需要通过 [选举配置更新](#选举配置更新) 流程来修改。

 ```js
config = {
  'gas_price'                :1000, /*1000MO，即十万分之一BU*/
  'base_reserve'             :1000000, /* 100万MO, 即0.01BU */
  'committee_size'           : 10,
  'kol_size'                 : 21,
  'kol_candidate_size'       : 100,
  'kol_min_pledge'           : 500000000000000,  /* 500 0000 0000 0000 */
  'validator_size'           : 19,
  'validator_candidate_size' : 100,
  'validator_min_pledge'     : 500000000000000,/* 500 0000 0000 0000 */
  'pledge_magnification'     : 2,
  'pass_rate'                : 0.5,
  'valid_period'             : 2592000000000,  /* 30 * 24 * 60 * 60 * 1000 * 1000 */
  'vote_unit'                : 1000000000, /*10 00000 00000*/
  'reward_allocation_share'  : [50,6,40,4],      /* validators 50%, validator candidates 6%, kol 10%, kol candidates 4% */
  'logic_contract'           : params.logic_contract
    };
```

|   参数  |    说明          | 默认值                                         |
| :----- | ------------------ | -------------------------------------------- |
| gas_price                    | 交易费燃料价格            |1000|
| base_reserve                 | 账户最小预留费用          |1000000|
| committee_size              | 委员会成员数目            |100|
| kol_size                    | 生态节点成员集合数目           |21|
| kol_candidate_size          | 生态节点候选人集合数目         |100|
| kol_min_pledge              | 生态节点候选人最小质押金额     |500000000000000|
| validator_size              | 共识节点集合数目          |19|
| validator_candidate_size    | 共识节点候选人集合数目     |100|
| validator_min_pledge        | 共识节点候选人最小质押金额  |500000000000000|
| pledge_magnification        | 质押金权益倍率             |2              |
| pass_rate                | 审核投票通过率。在有效审核期内，投票支持提案的委员的个数超过通过率，提案才会被执行。投票数 > (可投总数 * pass_rate) 则投票通过，例如，假设总共有4个节点，4 * 0.5 = 2, 投票数 > 2，那么至少要有3个投票才能通过。|0.5|
| valid_period                | 有效期，单位为微秒，应用在投票有效期以及退出锁定期|2592000000000|
|vote_unit                    |投票单元，每次投票、追加投票或竞选者追加质押金额必须为该值的整数倍。| 10 0000 0000|
| reward_allocation_share     |区块奖励的分配比例，[50,6,40,4] 代表共识节点集合平分区块奖励的 50%，共识节点的候选共识节点集合平分区块奖励的 6%，kol 集合平分区块奖励的 40%, 候选kol集合平分区块奖励的4%。|[50,6,40,4] |
|logic_contract |dpos的逻辑合约地址| "${logic_address}"|

### 用户角色

DPOS合约中支持的角色类型（任意 BuChain 账户可申请和担任多种角色）：

```js
role = [
  'committee',
  'validator',
  'kol'
];
```

注：所有接口参数中：committee代表委员会，validator代表共识节点，kol代表生态节点

`role`字段内列出的角色字符串，主要用于DPOS合约在执行申请(`apply`接口)、退出(`withdraw`接口)、废止(`abolish`接口)和审核(`approve`接口)操作时(详见[用户接口](#用户接口))，为接口的`role`参数提供可选参数值，以区别不同的角色类型。

### 提案动作

DPOS合约中提案的动作类型：

```js
motion = [
  'apply',
  'abolish',
  'withdraw',
  'config'
];
```

`motion`字段内列出的动作字符串，主要用于执行审核(`approve`接口)操作或清理过期提案(`clean`接口)操作时（详见[用户接口](#用户接口)），为接口的`operate`参数提供可选的参数值，以区别不同的提案动作。其中`withdraw`只会在清理过期提案(`clean`接口)操作时用到，退出提案不需要审核。

### 用户接口

#### 操作接口

|方法     |参数                          |说明 |
| :----- | ---------------------------- | -------------- |
|apply   |`role`、`pool`、`ratio`、`node`                |申请接口，任意BuChain账户申请成为候选共识节点、候选生态节点和委员会委员。`role`参数为申请的角色，参数值必须为[用户角色](#用户角色)章节中列出的值之一；`pool`为向投票用户分配区块奖励的地址，`ratio`为投票奖励的分配比率，合约会将该超级节点获取的区块奖励按照`ratio`指定的比率转入`pool`指定的地址，由该地址将奖励分配给所有支持此超级节点的投票用户，申请委员会委员不需要这两个参数；`node`为物理节点地址，只有申请的角色为候选共识节点时，此参数才需要赋值。|
|append  |`role`                       |追加质押金接口。候选共识节点或候选kol可以通过调用此接口追加质押金。`role`参数解释同上。|
|setNodeAddress|`address`                  |设置节点地址接口。共识节点或候选共识节点可以调用此接口设置出块的物理节点。`address`参数为新节点的地址。|
|setVoteDividend|`role`、`pool`、`ratio`    |设置投票奖励分配接口。超级节点及其候选节点可以调用此接口设置分配投票奖励的地址和分配比率。参数解释同apply接口。|
|abolish |`role`、`address`、`proof`    |废止接口。某角色成员提案废止同角色集合内另一成员。`address`参数为被废止者地址；`proof`参数为废止原因；`role`参数解释同上。|
|configure |`item`、`value`             |配置接口。委员会委员提案修改某项选举配置项的值。`item`参数为修改的配置项，参数值必须为[选举配置](#选举配置)章节列出的配置项之一；`value`参数为新配置值。|
|approve   |`operate`、`item`、`address`|审核接口。委员会成员对某提案审核后投票。`operate`参数为提案的动作类型，参数值必须为[提案动作](#提案动作)章节中列出的值之一；`item`参数为提案的项目，参数值必须为[用户角色](#用户角色)章节或[选举配置](#选举配置)章节列出的值之一；`address`参数为被提案者地址，如果审核的配置修改提案，则为提案者地址。|
|vote     |`role`、`address`            |投票接口。账户为候选共识节点和候选生态节点投票。`address`参数为投票支持的候选共识节点或候选生态节点，`role`参数解释同上。|
|unVote   |`role`、`address`            |撤销投票接口。已投票账户撤回选票。`role`参数和`address`参数解释同上。|
|withdraw |`role`                       | 退出接口。候选共识节点、候选生态节点或委员会委员退出自己所在集合。参数解释同上。|
|extract  |`list`                       | 提现接口。`list`为列表类型的参数，任意用户可调用此接口将指定地址集合的奖励提取到各自地址上。|
|clean    |`operate`、`item`、`address`  |清理过期提案接口。任意账户可以调用此接口清理任意过期提案。`operate`参数为提案的动作类型，参数值必须为[提案动作](#提案动作)章节中列出的值之一；`item`参数为提案的项目，参数值必须为[用户角色](#用户角色)章节或[选举配置](#选举配置)章节列出的值之一；`address`参数为被提案者地址，如果审核的配置修改提案，则为提案者地址。|

#### 查询接口

|方法     |参数                          |说明 |
| :----- | ---------------------------- | -------------- |
|getProposal             |`operate`、`item`、`address` |查询提案接口，任意BuChain账户可查询所有类型的提案信息，包括候选共识节点、候选生态节点和委员会委员的申请加入或废止提案，以及修改选举配置的提案。`operate`参数为提案的动作类型，参数值必须为[提案动作](#提案动作)章节中列出的值之一；`item`参数为提案的项目，参数值必须为[用户角色](#用户角色)章节或[选举配置](#选举配置)章节列出的值之一；`address`参数为被提案者地址，如果查询的是修改选举配置提案，则为提案者地址。|
|getVoteInfo            |`role`、`candidate`、`voter`  |查询投票接口。可查询某账户对某候选共识节点或候选生态节点的投票数。`candidate`参数为候选共识节点或候选生态节点的地址；`voter`为投票账户的地址，如果账户查询自己对某对象投出的票，`voter`参数可省略；`role`参数解释同上。|
|getValidators          |无                            |查询共识节点集合接口，可获取共识节点名单。|
|getValidatorCandidates |无                            |查询候选共识节点集合接口。可获取候选共识节点名单和各自的股东权益。|
|getKols                |无                            |查询生态节点集合接口。可获取生态节点名单。|
|getKolCandidates       |无                            |查询候选生态节点集合接口。可获取候选生态节点名单和各自的股东权益。|
|getCommittee           |无                            |查询委员会接口。可获取委员会名单。|
|getRewardDistribute    |无                            |查询区块奖励分配表接口。可获取所有候选共识节点和生态节点已分配的区块奖励数额。|
|getConfiguration       |无                            |查询选举配置接口。可获得所有可修改的选举配置项及其当前值。|

### 操作反馈
用户触发DPOS合约时，如果产生重要事件，DPOS合约将触发tlog交易，向用户反馈合约执行的结果。tlog交易会按顺序打印多个输出，其中第一个输出为事件名称，后面的输出为该事件的关键信息列表。

|事件     |输出                          |说明 |
| :----- | ---------------------------- | -------------- |
|addCandidate|`被加入者地址`、`角色`|申请者成功加入委员会或竞选列表时触发的事件。|
|deleteCandidate|`被删除者地址`、`角色`|委员会委员或竞选节点从所在的候选列表删除时触发的事件。|
|updateConfigure|`配置项`、`配置值`|选举相关配置变更时触发的事件。|
|penalty|`作恶者地址`、`角色`、`罚没数额`|作恶节点质押金被罚没时触发的事件。|

### 角色权职

不同角色以特定参数调用不同接口时，可以完成不同的操作。在DPOS合约中，不同角色可以执行的操作如下所示。

- user是账户的基础角色，每个账户都被视为user。用户可执行的操作如下：
  - 可申请成为委员会委员、候选共识节点和候选生态节点。
  - 可以为候选共识节点和候选生态节点投票。
  - 对投票支持过的候选共识节点或候选生态节点撤销投票。

  注：选票就是用户持有的BU，1BU为1票，默认投票单元为10BU，即投票金额必须为10U的倍数。用户可投票数为账户内可转账的BU数（账户最小预留费用不可投票）。单个用户可同时投票给多个候选共识节点和候选生态节点。用户投出的选票可全部撤回。

- committee 委员会委员可执行的操作如下：
  - 投票审核参选新委员申请、参选共识节点申请和参选生态节点申请。
  - 提案废止某个不称职委员，或提案变更某个系统配置。
  - 投票审核委员的废止提案、共识节点的废止提案、生态节点的废止提案或系统配置变更提案。

  注：委员会对所有提案和申请审核的原则是，一人一票，支持则投票，不支持则不投票，重复投票视为一票。在审核有效期内票数超过投票通过率（参见[选举配置](#选举配置)中的`pass_rate`配置项），则视为审核通过。如果截止到审核期结束，票数未超过投票通过率，则视为审核未通过。

- validator 共识节点可执行以下操作：
  - 提取区块奖励收益。
  - 提案废止某个不称职共识节点。
  - 退出共识节点集合。

  注：用户向DPOS合约提交参选共识节点申请，经委员会投票审核通过后，只能成为候选共识节点。只有股东权益（质押金和得票数之和）排名在限定名次（validator_size）内的，才可以成为正式的共识节点。

- 生态节点(Key Opinion Leader) 关键意见领袖可执行以下操作：
  - 提取区块奖励收益。
  - 提案废止某个不称职的生态节点。
  - 退出生态节点集合。

  注：用户向DPOS合约提交参选生态节点申请，经委员会投票审核通过后，只能成为候选生态节点。只有股东权益（质押金和得票数之和）排名在限定名次（kol_size）内的，才可以成为正式的生态节点。

## 共识节点选举

也被称为超级节点，是区块链共识系统的主要参与者。主要负责将一定时间段内的全网交易打包成提案，并就该提案达成一致，生成新的区块。共识节点由BuChain内所有账户投票选出。
共识节点的主要选举步骤如下：

- 任意节点向DPOS合约提交参选共识节点申请，并质押一定的BU用于防止作恶。
- [委员会](#委员会)对申请审核并投票，审核通过后，申请节点被加入候选共识节点集合。
- 用户对候选共识节点投票，得票达到一定名次的，成为共识节点，参与BuChain区块链的共识，并获得区块奖励。

### 申请成为候选共识节点

任意一个拥有网络节点的账户可以通过向DPOS合约转移一笔 BU 作为押金，申请成为候选共识节点。经委员会投票审核（参考[委员会批准投票](#委员会批准投票)）通过后，可成为正式的候选共识节点。但能否成为共识节点，是根据一定周期内获得的总票数决定的。

- 申请者向DPOS合约转移一笔 BU 作为押金（参见开发文档‘[转移BU资产](#转移bu资产)’），用户如果退出，该质押金将被锁定30天，超过锁定期后，再次申请退出，合约会将质押金全部返还申请账户（参见[候选共识节点退出](#候选共识节点退出)）。
- ‘转移货币’操作的 input 字段填入`{ "method" : "apply", "params":{"role":"validator", "pool":"此字段填入投票奖励分配地址", "ratio":"此字段填入投票奖励分配比率", "node":"此字段填入物理地址"}}`,注意使用转义字符。

|参数|描述
|:--- | ---
|role | 申请的角色，参数值必须为[用户角色](#用户角色)章节列出的值之一，此处为共识节点角色。
|pool | 投票奖励分配地址，如果参数未提供，默认为申请者地址。
|ratio | 投票奖励分配比率，值为百分数的分子，比如80，表示区块奖励的80%将转入pool参数指定的地址，再由pool地址分配给当前节点的支持者。如果参数未提供，默认为0。
|node| 节点地址，该地址为参与 BuChain 共识和出块的实际的物理节点的地址。为了保证账户资金安全，用户在申请共识节点时，资金地址和节点地址是分开的。

>例

```json
  "pay_coin" :
  {
    "dest_address" : "buQqzdS9YSnokDjvzg4YaNatcFQfkgXqk6ss",
    "amount" :300000000000000,
    "input":
    "{
      \"method\":\"apply\",
      \"params\":
      {
        \"role\":\"validator\", 
        \"pool\":\"buQhqMoJziz27DdrS4DaFjeUSspxetAfvpzu\",
        \"ratio\":0,
        \"node\":\"buQo8w52g2nQgxnfKWovUUEFQzMCTX5TRpZD\"
      }
    }"
  }
```

申请成功后可以通过[共识节点查询](#共识节点查询)，查询候选共识节点信息。

### 追加质押金

候选共识节点或候选生态节点可以追加质押金，提高自己的权益排名，追加额必须为[选举配置](#选举配置)中的`vote_unit`配置值的整数倍，否则追加操作将被拒绝。

- ‘转移货币’操作的 input 字段填入`{ "method" : "append", "params":{"role":"validator"}}`,注意使用转义字符。

|参数|描述
|:--- | ---
|role | 申请的角色，参数值必须为[用户角色](#用户角色)章节列出的值之一(committee不需要质押)，此处为共识节点角色。

>例

```json
  "pay_coin" :
  {
    "dest_address" : "buQqzdS9YSnokDjvzg4YaNatcFQfkgXqk6ss",
    "amount" :10000000000000,
    "input":
    "{
      \"method\":\"append\",
      \"params\":
      {
        \"role\":\"validator\"
      }
    }"
  }
```

### 切换节点地址

共识节点的资金地址和节点地址是分开的，为了方便共识节点在线更新升级，共识节点或候选共识节点可以调用此接口修改节点地址。

- 向DPOS合约转账 0 BU。
- ‘转移货币’操作的 input 字段填入`{ "method" : "setNodeAddress", "params" : {  "address" : "此处填入新节点地址"} }`，注意使用转义字符。

|参数|描述
|:--- | ---
|address | 新节点地址，用于代替当前执行出块任务的节点。

>例

```json
  "pay_coin" :
  {
    "dest_address" : "buQqzdS9YSnokDjvzg4YaNatcFQfkgXqk6ss",
    "amount" :0,
    "input":
    "{
      \"method\":\"setNodeAddress\",
      \"params\":
      {
        \"address\":\"buQo8w52g2nQgxnfKWovUUEFQzMCTX5TRpZD\"
      }
    }"
  }
```

### 设置投票奖励分配

超级节点调用此接口，可更改投票奖励分配地址和比率。

- 向DPOS合约转账0BU。
- ‘转移货币’操作的 input 字段填入`{ "method" : "setVoteDividend", "params":{"role":"此处填入超级节点的角色类型", "pool":"此字段填入投票奖励分配地址", "ratio":"此字段填入投票奖励分配比率"}}`,注意使用转义字符。

|参数|描述
|:--- | ---
|role | 申请的角色，参数值必须为[用户角色](#用户角色)章节列出的值之一，但不能是委员会。
|pool | 投票奖励分配地址。
|ratio | 投票奖励分配比率，值为百分数的分子，比如80，表示区块奖励的80%将转入pool参数指定的地址，再由pool地址分配给当前节点的支持者。

>例

```json
  "pay_coin" :
  {
    "dest_address" : "buQqzdS9YSnokDjvzg4YaNatcFQfkgXqk6ss",
    "amount" :0,
    "input":
    "{
      \"method\":\"setVoteDividend\",
      \"params\":
      {
        \"role\":\"validator\", 
        \"pool\":\"buQhqMoJziz27DdrS4DaFjeUSspxetAfvpzu\",
        \"ratio\":80
      }
    }"
  }
```

### 为候选共识节点投票

- 任意用户向DPOS合约转账一笔BU，转账额视为用户的投票数，在转账参数中提供的地址，视为投票支持的候选共识节点。
- 候选共识节点的得票总数为自身质押额与得票数之和，候选共识节点增加质押额相当于给自己投票。
- 用户可以为多个候选地址投票，可投票的候选共识节点个数，取决于候选共识节点集合大小和用户的账户余额。
- 对同一地址重复投票，视为增加投票。
- 投票数和追加额必须为[选举配置](#选举配置)中`vote_unit`配置值的整数倍。
- ‘转移货币’操作的 input 字段填入`{ "method" : "vote", "params" : { "role":"validator", "address" : "填入候选共识节点地址"} }`，注意使用转义字符。

|参数|描述
|:--- | --- 
|role | 被投票者的角色，参数值必须为[用户角色](#用户角色)章节列出的值之一，此处为共识节点角色。
|address| 被投票者的地址。

>例：对指定候选共识节点投票

```json
  "pay_coin" :
  {
    "dest_address" : "buQqzdS9YSnokDjvzg4YaNatcFQfkgXqk6ss",
    "amount" :100000000000, /*投票1000BU*/
    "input":
    "{
        \"method\":\"vote\",
        \"params\":
        {
          \"role\":\"validator\",
          \"address\":\"buQtZrMdBQqYzfxvqKX3M8qLZD3LNAuoSKj4\"
        }
    }"
  }
```

### 撤销投票

- 向DPOS合约转账 0 BU。
- ‘转移货币’操作的 input 字段填入`{ "method" : "unVote", "params" : { "role":"validator", "address" : "填入候选共识节点地址"} }`，注意使用转义字符。
- 投票信息记录在合约中，可以通过获取投票信息接口getVoteInfo查询。

|参数|描述
|:--- | --- 
|role |  被投票者的角色，参数值必须为[用户角色](#用户角色)章节列出的值之一，此处为共识节点角色。
|address|  被投票者的地址。

```json
  "pay_coin" :
  {
    "dest_address" : "buQqzdS9YSnokDjvzg4YaNatcFQfkgXqk6ss",
    "amount" :0,
    "input":
    "{
        \"method\":\"unVote\",
        \"params\":
        {
          \"role\":\"validator\",
          \"address\":\"buQtZrMdBQqYzfxvqKX3M8qLZD3LNAuoSKj4\"
        }
    }"
  }
```

### 候选共识节点退出

- 候选共识节点可通过此操作退出候选共识节点，并收回全部押金。退出流程分两步：
  - 第一步是申请退出，申请成功后，节点地址在候选节点集合中被删除，质押金进入退出锁定期，锁定期为30天。
  - 锁定期结束后进入第二步，可以再次发送退出申请，此时锁定期已过，DPOS合约账户将质押金返还原账户。

- 向DPOS合约转账 0 BU。
- ‘转移资产’或‘转移货币’操作的 input 字段填入 `{ "method":"withdraw", "params" :{ "role":"validator" }}`，注意使用转义字符。

|参数|描述
|:--- | ---
|role |  退出者的角色，参数值必须为[用户角色](#用户角色)章节列出的值之一，此处为共识节点角色。

>例

```json
  "pay_coin" :
  {
    "dest_address" : "buQqzdS9YSnokDjvzg4YaNatcFQfkgXqk6ss",
    "amount" :0,
    "input":
    "{
      \"method\":\"withdraw\",
      \"params\":{
        \"role\":\"validator\"
      }
    }"
  }
```

### 废止恶意共识节点提案

如果某共识节点发现有另一个共识节点为恶意节点，或者不再适合作为共识节点，可以申请废止该恶意节点。发起‘废止恶意节点’提案后，需要由委员会投票决定是否执行废止操作。

- 废止者向DPOS合约转账 0 BU。
- ‘转移资产’或‘转移货币’操作的 input 字段填入 `{ "method" : "abolish",  "params" : { "role":"validator", "address" : "此处填入恶意共识节点地址", "proof" : "此处填入废止该共识节点的原因"} }`，注意使用转义字符。

|参数|描述
|:--- | ---
|role |  被废止者的角色，参数值必须为[用户角色](#用户角色)章节列出的值之一，此处为共识节点角色。
|address|  被废止者的地址。
|proof|  废止的原因。

>例

```json
  "pay_coin" :
  {
    "dest_address" : "buQqzdS9YSnokDjvzg4YaNatcFQfkgXqk6ss",
    "amount" :0,
    "input":
    "{
      \"method\":\"abolish\",
      \"params\":
      {
        \"role\":\"validator\",
        \"address\":\"buQmvKW11Xy1GL9RUXJKrydWuNykfaQr9SKE\"，
        \"proof\":\"I_saw_it_uncomfotable.\"
      }
    }"
  }
```

注意：申请废止者和被废止者必须都是共识节点。

### 共识节点查询

用户通过向查询接口（即 query 接口）提供指定参数，可以查看相关信息, 调用查询接口当前只能通过 callContract接口。 contract_address 字段填入DPOS合约账户地址。

#### 查询当前共识节点集合

>例

```json
  {
    "contract_address" : "buQqzdS9YSnokDjvzg4YaNatcFQfkgXqk6ss",
    "code" : "",
    "input" : "{\"method\": \"getValidators\"}",
    "opt_type" : 2,
    "source_address" : ""
  }
```

> 查询结果

```json
{
  "result": {
      "type": "string",
      "value": "{\"validators\":[
        [\"buQWT7vkMth2y9RHgSdqqw77sCybgWRsB7jM\",\"500000000000000\"],[\"buQBwe7LZYCYHfxiEGb1RE9XC9kN2qrGXWCY\",\"500000000000000\"],[\"buQWBgAWSqiES7TNh1mq2VQwonvWtESz8Z2Z\",\"500000000000000\"],[\"buQWQ4rwVW8RCzatR8XnRnhMCaCeMkE46qLR\",\"500000000000000\"],[\"buQrVDKPCVE6LfCf8TyZEaiZ8R99NrSn4Fuz\",\"500000000000000\"]]}"
  }
}
```

#### 查询候选共识节点集合信息

>例

```json
  {
    "contract_address" : "buQqzdS9YSnokDjvzg4YaNatcFQfkgXqk6ss",
    "code" : "",
    "input" : "{\"method\": \"getValidatorCandidates\"}",
    "opt_type" : 2,
    "source_address" : ""
  }
```

> 查询结果

```json
{
  "result": {
      "type": "string",
      "value": "{\"validator_candidates\":[
        [\"buQWT7vkMth2y9RHgSdqqw77sCybgWRsB7jM\",\"502500000000000\"],[\"buQBwe7LZYCYHfxiEGb1RE9XC9kN2qrGXWCY\",\"501500000000000\"],[\"buQWBgAWSqiES7TNh1mq2VQwonvWtESz8Z2Z\",\"500500000000000\"],[\"buQWQ4rwVW8RCzatR8XnRnhMCaCeMkE46qLR\",\"500000000000000\"],[\"buQrVDKPCVE6LfCf8TyZEaiZ8R99NrSn4Fuz\",\"500000000000000\"]]}"
  }
}
```

#### 查询用户的共识节点投票信息

|参数|描述
|:--- | --- 
|voter |  投票者地址，如果用户查询自己的投票信息，可以省略。
|role|  被投票者的角色，参数值必须为[用户角色](#用户角色)章节列出的值之一，此处为共识节点。
|candidate|  被投票者的地址。

>例

```json
  {
    "contract_address" : "buQqzdS9YSnokDjvzg4YaNatcFQfkgXqk6ss",
    "code" : "",
    "input" :
    "{
      \"method\": \"getVoteInfo\",
      \"params\":
      {
         \"voter\":\"buQrVDKPCVE6LfCf8TyZEaiZ8R99NrSn4Fuz\",
         \"role\": \"validator\",
         \"candidate\": \"buQmvKW11Xy1GL9RUXJKrydWuNykfaQr9SKE\"
      }
    }",
    "opt_type" : 2,
    "source_address" : ""
  }
```

> 查询结果

```json
  {
    "result": {
        "type": "string",
        "value": "{\"voterInfo\":500000000000}"
    }
  }
```

#### 查询共识节点申请信息

|参数|描述
|:--- | ---
|operate |  提案动作，参数值必须为[提案动作](#提案动作)章节列出的值之一，此处为申请动作。
|item|  提案的项目，参数值必须为[用户角色](#用户角色)章节或[选举配置](#选举配置)章节列出的值之一，此处为共识节点角色。
|address|  申请者的地址。

>例

```json
  {
    "contract_address" : "buQqzdS9YSnokDjvzg4YaNatcFQfkgXqk6ss",
    "code" : "",
    "input" :
    "{
      \"method\": \"getProposal\",
      \"params\":
      {
        {
          \"operate\": \"apply\",
          \"item\":\"validator\",
          \"address\":\"buQmvKW11Xy1GL9RUXJKrydWuNykfaQr9SKE\"
      }
    }",
    "opt_type" : 2,
    "source_address" : ""
  }
```

>查询结果

|参数|类型|描述
|:--- | --- |---
|proposal | string |  提案
|pledge| string |质押金额
|expiration| number | 过期时间
|ballot| array | 委员会成员支持该提案的投票列表
|passTime| number | 委员会审核通过时间，未通过则无该字段

```json
{
  "result": {
      "type": "string",
      "value": "{\"proposal\":{\"pledge\":\"500000000000000\",\"expiration\":1552098925001842,\"ballot\":[\"buQmKmaeCyGcPk9KbvnkhpLzQa34tQ9MaWwt\",\"buQYKj4TTJPVDPXCLWeBZMoCr1JPhq9Z2tJm\",\"buQZoJk8bq6A1AtsmfRw3rYJ79eMHUyct9i2\"],\"passTime\":1550802935024539}}"
  }
}
```

#### 查询指定的废止恶意节点提案的信息

|参数|描述
|:--- | ---
|operate |  提案动作，参数值必须为[提案动作](#提案动作)章节列出的值之一，此处废止动作。
|item|  提案的项目，参数值必须为[用户角色](#用户角色)章节或[选举配置](#选举配置)章节列出的值之一，此处为共识节点角色。
|address|  被废止者的地址。

>例

```json
  {
    "contract_address" : "buQqzdS9YSnokDjvzg4YaNatcFQfkgXqk6ss",
    "code" : "",
    "input" :
    "{
      \"method\": \"getProposal\",
      \"params\":
      {
        {
          \"operate\": \"abolish\",
          \"item\":\"validator\",
          \"address\":\"buQmvKW11Xy1GL9RUXJKrydWuNykfaQr9SKE\"
      }
    }",
    "opt_type" : 2,
    "source_address" : ""
  }
```

>查询结果

|参数|类型|描述
|:--- | --- |--- 
|proposal | string |  提案
|informer| string |举报者地址
|reason| string | 举报原因
|expiration| number | 过期时间
|ballot| array | 委员会成员支持该提案的投票列表

```json
{
  "result": {
      "type": "string",
      "value": "{\"proposal\":{\"informer\":\"buQWQ4rwVW8RCzatR8XnRnhMCaCeMkE46qLR\",\"reason\":\"see abnormal record\",\"expiration\":1550815129920811,\"ballot\":[\"buQWQ4rwVW8RCzatR8XnRnhMCaCeMkE46qLR\"]}}"
  }
}
```

## 委员会

委员会是独立于候选共识节点和共识节点之外的决策层，不参与奖励分配，没有利益驱动，成员主要来自于基金会和核心开发者，选举和创建时指定一个委员会集合，之后新成员的加入和退出需要其他委员会成员的投票决定。

目前主要功能有：

- 选举配置更新，如果有委员成员觉得当前选举配置不合理，可以提出选举配置更新提案，在有效期内委员会成员投票通过之后，下一轮区块打包时则使用新的选举配置。
- 候选共识节点审核，普通节点申请成为候选共识节点时，由委员会成员审核其是否具备成为候选共识节点的条件，包括物理节点配置，个人或者组织认证信息，信用程度等。
- 生态节点成员的审核，普通用户申请成为生态节点时，由委员会审核其是否具备成为生态节点的条件，包括公众影响力，社区贡献，以及个人或者组织认证信息，信用程度等。
- dpos逻辑功能合约更新。

### 委员会成员初始化设置

委员会成员初始化操作在合约创建时在入口函数init()中完成。
初始化之前需要将委员会成员在社区公示，以接受用户的监督，提高公信力。

### 委员会选举

#### 申请加入委员会

- 用户向DPOS合约转账 0 BU，申请成为新委员。成为新得委员需获得当前委员会投票审核（参考[委员会批准投票](#委员会批准投票)）通过。
- ‘转移货币’操作的 input 字段填入 { "method" : "apply", "params" : {"role":"committee"}}，注意使用转义字符。

|参数|描述
|:--- | ---
|role |  申请的角色，参数值必须为[用户角色](#用户角色)章节列出的值之一，此处为委员会委员。

>例

```json
  "pay_coin" :
  {
    "dest_address" : "buQqzdS9YSnokDjvzg4YaNatcFQfkgXqk6ss",
    "amount" :0,
    "input":
    "{
      \"method\":\"apply\"，
      \"params\" : {
        \"role\": \"committee\"
      }
    }"
  }
```

申请成功后可以通过[委员会查询](#委员会查询)，查询候选共识节点信息。

### 委员会批准投票

- 需要经过委员会的审核和批准的提案包含：候选共识节点，候选生态节点和委员会新成员的加入/废除，以及配置更新。 多数委员会成员批准通过后（参见[选举配置](#选举配置)的`pass_rate`配置项）提案才会执行。审核投票的时候需要指定提案动作类型、被提案者角色和被提案者地址，如果审核的是配置更新提案，则使用提案的[选举配置](#选举配置)项代替角色、提案者地址代替被提案者地址（更新配置提案无被提案者）。
- 委员会成员向DPOS合约转账 0 BU。
- ‘转移货币’操作的 input 字段填入 `{ "method":"approve", "params" : {"item": "committee", "address": "此处填入加入或废除的地址", "operate": "此处填入提案类型"} }`，注意使用转义字符。

|参数|描述
|:--- | ---
|operate |  提案动作，参数值必须为[提案动作](#提案动作)章节列出的值之一，此处为申请动作。
|item| 提案的项目，参数值必须为[用户角色](#用户角色)章节或[选举配置](#选举配置)章节列出的值之一，此处为委员会委员角色。
|address| 申请者的地址。

>例

```json
  "pay_coin" :
  {
    "dest_address" : "buQqzdS9YSnokDjvzg4YaNatcFQfkgXqk6ss",
    "amount" :0,
    "input":"{
      \"method\":\"approve\",
      \"params\" : {
        \"item\":\"committee\",
        \"address\": \"buQZoJk8bq6A1AtsmfRw3rYJ79eMHUyct9i2\"，
        \"operate\": \"apply\"
      }
    }"
  }
```

- 委员会在有效审核期内（参见[选举配置](#选举配置)的`valid_period`配置项）投票超过审核通过率，则审核通过。如果审核的是申请提案，投票通过后，申请者被加入对应集合。如果审核的是废止提案，则被提案者从对应集合内被删除。如果审核的是配置更新提案，则以提案内的值取代原配置项值。

#### 委员会成员退出

- 向DPOS合约转账 0 BU。
- ‘转移资产’或‘转移货币’操作的 input 字段填入 `{ "method":"withdraw", "params" : {"role":"committee"} }`，注意使用转义字符。
- 委员会成员主动退出无需其他成员批准。

|参数|描述
|:--- | ---
|role |  退出者的角色，参数值必须为[用户角色](#用户角色)章节列出的值之一，此处为委员会委员。

>例

```json
  "pay_coin" :
  {
    "dest_address" : "buQqzdS9YSnokDjvzg4YaNatcFQfkgXqk6ss",
    "amount" :0,
    "input":"{
      \"method\":\"withdraw\",
      \"params\" : {
        \"role\":\"committee\"
      }
    }"
  }
```

#### 委员会查询

用户通过向查询接口（即 query 接口）提供指定参数，可以查看相关信息, 调用查询接口当前只能通过 callContract, contract_address 字段填入DPOS合约账户地址。

>例

```json
  {
    "contract_address" : "buQqzdS9YSnokDjvzg4YaNatcFQfkgXqk6ss",
    "code" : "",
    "input" : "{\"method\": \"getCommittee\"}",
    "opt_type" : 2,
    "source_address" : ""
  }
```

>查询结果

```json
{
  "result": {
      "type": "string",
      "value": "{\"committee\":[\"buQZoJk8bq6A1AtsmfRw3rYJ79eMHUyct9i2\",\"buQYKj4TTJPVDPXCLWeBZMoCr1JPhq9Z2tJm\",\"buQcYkkoZFMwDNQgCD7DoykNZjtax4FjVSzy\",\"buQmKmaeCyGcPk9KbvnkhpLzQa34tQ9MaWwt\"]}"
  }
}
```

### 选举配置更新

- DPOS合约不仅支持选举功能，也支持选举配置的更新，选举配置更新需要委员会投票确认。在有效期内确认完成后，将触发选举配置的更新，从下一区块开始采用新的配置。

#### 选举配置结构

- 可调整的选举配置，如果在运营者发现配置值不合理，可以通过委员会提案和投票修改。

#### 选举配置更新提案

- 委员会成员向DPOS合约转账 0 BU。
- 委员会委员可以提议更新某一个参数，只需要在配置中填入需要更新的参数即可。
- ‘转移资产’或‘转移货币’操作的 input 字段填入 { "method" : "configure",  "params" : { "item" :"kol_min_pledge", "value": "此处填入生态节点最低质押金额"} }，注意使用转义字符。

|参数|描述
|:--- | ---
|method | 调用合约内部的指定功能，此处为configure
|item   | 提案的配置项，参数值必须为[选举配置](#选举配置)章节列出的值之一，此处为生态节点最小质押额。
|value  | 待修改配置项的值

>例

```json
  "pay_coin" :
  {
    "dest_address" : "buQqzdS9YSnokDjvzg4YaNatcFQfkgXqk6ss",
    "amount" :0,
    "input":
    "{
      \"method\":\"configure\",
      \"params\":
      {
        \"item\": \"kol_min_pledge\",
        \"value\": 2000000000000
      }
    }"
  }
```

注意：只有委员会成员拥有投票权。若有效期内该配置更新提案未投票通过，则提案作废，选举配置保持不变。

### 查询选举配置信息

用户通过向查询接口（即 query 接口）提供指定参数，可以查看相关信息, 调用查询接口当前只能通过 callContract, contract_address 字段填入DPOS合约账户地址。

>例

```json
  {
    "contract_address" : "buQqzdS9YSnokDjvzg4YaNatcFQfkgXqk6ss",
    "code" : "",
    "input":"{\"method\": \"getConfiguration\"}",
    "opt_type" : 2,
    "source_address" : ""
  }
```

>查询 结果

```json
{
  "result": {
      "type": "string",
      "value": "{\"configuration\":{\"gas_price\":1000,\"base_reserve\":1000000,\"committee_size\":10,\"kol_size\":21,\"kol_candidate_size\":100,\"kol_min_pledge\":300000000000000,\"validator_size\":19,\"validator_candidate_size\":100,\"validator_min_pledge\":300000000000000,\"pledge_magnification\":2,\"pass_rate\":0.5,\"valid_period\":2592000000000,\"vote_unit\":1000000000,\"reward_allocation_share\":[50,8,35,7],\"logic_contract\":\"buQf9SYLbfiCBFHkwNhNAnnFgS2o2AraRxoe\"}}"
  }
}
```

## 社区激励

公链生态的发展离不开社区的活跃，对生态节点进行奖励是一个提升公链知名度，提升关注度，增加社区成员的办法。

### 生态节点申请
任意一个用户账户可以通过向DPOS合约转移一笔 BU 作为押金，申请成为候选生态节点，只有在有效期内获得委员会审核通过，才能加入候选生态节点列表，参考[委员会批准投票](#委员会批准投票)。能否成为正式生态节点，是根据一定周期内获得的用户投票总票数决定的。

- 申请者向DPOS合约转移一笔 BU 作为押金（参见开发文档‘[转移BU资产](#转移bu资产)’），该押金可通过 ‘[收回押金](#收回押金)’ 操作收回。
- ‘转移货币’操作的 input 字段填入`{ "method" : "apply", "params":{"role":"kol", "pool":"此字段填入投票奖励分配地址", "ratio":"此字段填入投票奖励分配比率"}}`，注意使用转义字符。

|参数|描述
|:--- | ---
|role | 申请的角色，参数值必须为[用户角色](#用户角色)章节列出的值之一，此处为共识节点角色。
|pool | 投票奖励分配地址，如果参数未提供，默认为申请者地址。
|ratio | 投票奖励分配比率，值为百分数的分子，比如80，表示区块奖励的80%将转入pool参数指定的地址，再由pool地址分配给当前节点的支持者。如果参数未提供，默认为0。

>例

```json
  "pay_coin" :
  {
    "dest_address" : "buQqzdS9YSnokDjvzg4YaNatcFQfkgXqk6ss",
    "amount" :300000000000000,
    "input":"{
      \"method\":\"apply\",
      \"params\" : {
        \"role\":\"kol\",
        \"pool\":\"buQhqMoJziz27DdrS4DaFjeUSspxetAfvpzu\",
        \"ratio\":0,
      }
    }"
  }
```

申请成功后可以通过[查询当前生态节点集合信息](#查询当前生态节点集合信息)接口，查询候选生态节点信息。

### 生态节点退出

- 生态节点可通过此操作收回全部押金。退出流程分两步：
  - 第一步是申请退出，申请成功后，节点地址在候选节点集合中被删除，质押金进入退出锁定期，锁定期为30天。
  - 锁定期结束后进入第二步，可以再次发送退出申请，此时锁定期已过，DPOS合约账户将质押金返还原账户。
- 向DPOS合约转账 0 BU。
- ‘转移资产’或‘转移货币’操作的 input 字段填入 `{ "method":"withdraw", "params" : {"role":"kol"}}`，注意使用转义字符。

|参数|描述
|:--- | ---
|role | 退出者的角色，参数值必须为[用户角色](#用户角色)章节列出的值之一，此处为生态节点。

>例

```json
  "pay_coin" :
  {
    "dest_address" : "buQqzdS9YSnokDjvzg4YaNatcFQfkgXqk6ss",
    "amount" :0,
    "input":"{
      \"method\":\"withdraw\",
      \"params\" : {
        \"role\":\"kol\"
      }
    }"
  }
```

### 生态节点投票和撤销投票

- 所有用户均可向DPOS合约转移一笔BU投票支持某个候选生态节点。
- 用户可以为多个候选生态节点投票，可投票的候选生态节点个数，取决于候选生态节点集合大小和用户的账户余额。
- 对同一地址重复投票，视为增加投票。
- 投票数和追加额必须为[选举配置](#选举配置)中`vote_unit`配置值的整数倍。
- ‘转移货币’操作的 input 字段填入 `{ "method":"vote", "params" : {"role":"kol", "address": "此处填入候选生态节点的地址"}}`，注意使用转义字符。

>投票

|参数|描述
|:--- | ---
|role | 被投票者的角色，参数值必须为[用户角色](#用户角色)章节列出的值之一，此处为生态节点角色。
|address| 被投票者的地址。

```json
  "pay_coin" :
  {
    "dest_address" : "buQqzdS9YSnokDjvzg4YaNatcFQfkgXqk6ss",
    "amount" :1000000000,
    "input":"{
      \"method\":\"vote\",
      \"params\" : {
        \"role\":\"kol\",
        \"address\": \"buQYKj4TTJPVDPXCLWeBZMoCr1JPhq9Z2tJm\"
      }
    }"
  }
```

>撤销投票

|参数|描述
|:--- | ---
|role | 被投票者的角色，参数值必须为[用户角色](#用户角色)章节列出的值之一，此处为生态节点角色。
|address| 被投票者的地址。

```json
  "pay_coin" :
  {
    "dest_address" : "buQqzdS9YSnokDjvzg4YaNatcFQfkgXqk6ss",
    "amount" :0,
    "input":"{
      \"method\":\"unVote\",
      \"params\" : {
        \"role\":\"kol\",
        \"address\": \"buQYKj4TTJPVDPXCLWeBZMoCr1JPhq9Z2tJm\"
      }
    }"
  }
```

#### 查询生态节点申请信息

|参数|描述
|:--- | ---
|operate | 提案动作，参数值必须为[提案动作](#提案动作)章节列出的值之一，此处为申请动作。
|item| 提案的项目，参数值必须为[用户角色](#用户角色)章节或[选举配置](#选举配置)章节列出的值之一，此处为生态节点角色。
|address|申请者的地址。

>例

```json
  {
    "contract_address" : "buQqzdS9YSnokDjvzg4YaNatcFQfkgXqk6ss",
    "code" : "",
    "input" :"{
      \"method\": \"getProposal\",
      \"params\":{
        \"operate\": \"apply\",
        \"item\":\"kol\",
        \"address\":\"buQmvKW11Xy1GL9RUXJKrydWuNykfaQr9SKE\"
      }
    }",
    "opt_type" : 2,
    "source_address" : ""
  }
```

>查询结果

```json
{
  "result": {
    "type": "string",
    "value": "{\"proposal\":{\"pledge\":\"5000000000000\",\"expiration\":1550816576664577,\"ballot\":[\"buQmKmaeCyGcPk9KbvnkhpLzQa34tQ9MaWwt\",\"buQYKj4TTJPVDPXCLWeBZMoCr1JPhq9Z2tJm\",\"buQZoJk8bq6A1AtsmfRw3rYJ79eMHUyct9i2\"],\"passTime\":1550816546664577}}"
}
```

### 查询用户的生态节点投票信息

|参数|描述
|:--- | ---
|voter | 投票者地址，如果用户查询自己的投票信息，可以省略。
|role| 被投票者的角色，参数值必须为[用户角色](#用户角色)章节列出的值之一，此处为生态节点。
|candidate|被投票者的地址

>例

```json
  {
    "contract_address" : "buQqzdS9YSnokDjvzg4YaNatcFQfkgXqk6ss",
    "code" : "",
    "input" :
    "{
      \"method\": \"getVoteInfo\",
      \"params\":
      {
         \"voter\":\"buQrVDKPCVE6LfCf8TyZEaiZ8R99NrSn4Fuz\",
         \"role\": \"kol\",
         \"candidate\": \"buQmvKW11Xy1GL9RUXJKrydWuNykfaQr9SKE\"
      }
    }",
    "opt_type" : 2,
    "source_address" : ""
  }
```

>查询结果

```json
{
  "result": {
      "type": "string",
      "value": "{\"voterInfo\":500000000000}"
  }
}
```

### 查询当前生态节点集合信息

用户通过向查询接口（即 query 接口）提供指定参数，可以查看相关信息, 调用查询接口当前只能通过 callContract, contract_address 字段填入DPOS合约账户地址。

>例

```json
  {
    "contract_address" : "buQqzdS9YSnokDjvzg4YaNatcFQfkgXqk6ss",
    "code" : "",
    "input" : "{\"method\": \"getKols\"}",
    "opt_type" : 2,
    "source_address" : ""
  }
```

>查询结果

```json
{
  "result": {
      "type": "string",
      "value": "{\"kols\":[
        [\"buQB3LtCXfLjtSJKfpaHpykEwDLf43nPxB6z\",\"605000000000000\"],[\"buQZayH6gcAFh5XdgS4tnn8Axrqo1NdutS3p\",\"600000000000000\"],[\"buQaUqDotGNM7htvPR6iHKHBxLGzVpSFkmBM\",\"600000000000000\"]]}"
  }
}
```

### 查询候选生态节点集合信息

>例

```json
  {
    "contract_address" : "buQqzdS9YSnokDjvzg4YaNatcFQfkgXqk6ss",
    "code" : "",
    "input" : "{\"method\": \"getKolCandidates\"}",
    "opt_type" : 2,
    "source_address" : ""
  }
```

>查询结果

```json
{
  "result": {
      "type": "string",
      "value": "{\"kol_candidates\":[
        [\"buQB3LtCXfLjtSJKfpaHpykEwDLf43nPxB6z\",\"605000000000000\"],[\"buQZayH6gcAFh5XdgS4tnn8Axrqo1NdutS3p\",\"600000000000000\"],[\"buQaUqDotGNM7htvPR6iHKHBxLGzVpSFkmBM\",\"600000000000000\"]]}"
  }
}
```

### 查询节点奖励信息

>例

```json
  {
    "contract_address" : "buQqzdS9YSnokDjvzg4YaNatcFQfkgXqk6ss",
    "code" : "",
    "input" : "{\"method\": \"getRewardDistribute\"}",
    "opt_type" : 2,
    "source_address" : ""
  }
```

>查询结果

Reward列表按key-value形式存储

|参数|描述|
|:--- | --- |
|key | 超级节点地址。|
|value | value[0]为奖励金额，value[1]为投票奖励分配地址，value[2]为投票奖励分配比率。|

```json
{
  "result": {
    "type": "string",
    "value": "{
      \"rewards\":{
        \"validators\":{
          \"buQa6cifJPAduxqWf42dUgZ72qQiLGr4VCyn\":[\"10013634880\",\"buQa6cifJPAduxqWf42dUgZ72qQiLGr4VCyn\",0]
        },
        \"kols\":{
          \"buQhqMoJziz27DdrS4DaFjeUSspxetAfvpzu\":[\"282345624\",\"buQhqMoJziz27DdrS4DaFjeUSspxetAfvpzu\",0],
          \"buQmziFKwWTvJe5hBCFa6e3FdPDPorht3wCB\":[\"282345624\",\"buQmziFKwWTvJe5hBCFa6e3FdPDPorht3wCB\",0],
          \"buQnP4jueoC37fP2VxKpsZSTNjgFHpA442jy\":[\"282345624\",\"buQnP4jueoC37fP2VxKpsZSTNjgFHpA442jy\",0],
          \"buQjxgZsG3B24PNzKzpTiwrRR3z6ok46izuR\":[\"282345624\",\"buQjxgZsG3B24PNzKzpTiwrRR3z6ok46izuR\",0],
          \"buQhtTjDmmquE4PTAKSzx4CqL7Z5LKLVG8SZ\":[\"282345624\",\"buQhtTjDmmquE4PTAKSzx4CqL7Z5LKLVG8SZ\",0]
        }
      }
    }"
  }
}
```

## 其他接口

### 奖励提取

- 任意用户调用此接口可以为指定的地址集合领取区块奖励，如果有领取地址不在奖励收益列表中，则该地址收不到奖励。
- 向DPOS合约转账0BU。
- ‘转移资产’或‘转移货币’操作的 input 字段填入 `{ "method":"extract", "params":{"list":[此处填入提取奖励的地址集合]}}`，注意使用转义字符。

|参数|描述
|:--- | ---
|list | 领取奖励地址列表，为数组类型，领取奖励地址的个数不得超过100个，如果参数未提供，则默认领取合约触发者的奖励。

>奖励取现

```json
  "pay_coin" :
  {
    "dest_address" : "buQqzdS9YSnokDjvzg4YaNatcFQfkgXqk6ss",
    "amount" :0,
    "input":"{
      \"method\":\"extract\",
      \"params\":{
        \"list\":[
          \"buQrVDKPCVE6LfCf8TyZEaiZ8R99NrSn4Fuz\",
          \"buQmziFKwWTvJe5hBCFa6e3FdPDPorht3wCB\",
          \"buQmvKW11Xy1GL9RUXJKrydWuNykfaQr9SKE\",
          \"buQnP4jueoC37fP2VxKpsZSTNjgFHpA442jy\",
          \"buQjxgZsG3B24PNzKzpTiwrRR3z6ok46izuR\"
      ]}
    }"
  }
```

### 清理过期提案

对于所有类型的过期提案（包括共识节点、生态节点、委员会委员的申请、退出和废止，以及所有类型的选举配置更新提案），任意用户可触发此接口清理该过期提案。

- 向DPOS合约转账0BU。
- ‘转移货币’操作的 input 字段填入 `{ "method":"clean", "params" : {"item": "此处填入提案项目", "address": "此处填入提案者或被提案者地址", "operate": "此处填入提案类型"} }`，注意使用转义字符。
- 如果被清理的过期提案为申请提案或退出提案，且提案涉及质押金，则该提案被清理时，质押金将被退还原账户。

>例

```json
  "pay_coin" :
  {
    "dest_address" : "buQqzdS9YSnokDjvzg4YaNatcFQfkgXqk6ss",
    "amount" :0,
    "input":"{
      \"method\":\"clean\",
      \"params\" : {
        \"item\":\"kol_min_pledge\",
        \"address\": \"buQZoJk8bq6A1AtsmfRw3rYJ79eMHUyct9i2\"，
        \"operate\": \"config\"
      }
    }"
  }
```