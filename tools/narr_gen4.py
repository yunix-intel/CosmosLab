# 解说词第四批: B.5 热历史早期三节点 + ISM/星团链 (14 段)
# ID: evo.cos.b0-b2 (before s0, 时间轴画不下, 只做文本讲解)
#     evo.ism.s0-s10 (新剧本"星际介质与星团" 11 段)
import json

OUT = "D:/tmp/solar-system-cpp/tools/_narr_part4.json"
N = {}
def put(i, zh, yue, ja, en):
    N[i] = {"zh": zh, "yue": yue, "ja": ja, "en": en}

put("evo.cos.b0",
"普朗克时期: 10的负43次方秒之前, 量子引力未知 —— 明确标理论空白, 教科书从这里开始保持沉默。",
"普朗克时期: 10嘅负43次方秒之前, 量子引力未知 —— 明确标理论空白, 教科书由呢度开始保持沉默。",
"プランク時代:10の-43乗秒以前、量子重力は未知——理論の空白と明示します。教科書はここから沈黙します。",
"Planck epoch: before 10^-43 s, quantum gravity is unknown — marked explicitly as theoretical blank. Textbooks fall silent here.")
put("evo.cos.b1",
"暴胀: 10的负32次方秒量级, 解决视界、平坦、单极问题。但它是假说 —— 原初引力波B模至今未探测到, 别当定论讲。",
"暴胀: 10嘅负32次方秒量级, 解决视界、平坦、单极问题。但佢系假说 —— 原初引力波B模至今未探测到, 唔好当定论讲。",
"インフレーション:10の-32乗秒級、地平線・平坦性・モノポール問題を解決します。但し仮説です——原始重力波Bモードは未検出であり、定説として扱えません。",
"Inflation: at ~10^-32 s, solving horizon, flatness, monopole problems. But it is a hypothesis — primordial B-modes remain undetected. Do not teach as settled.")
put("evo.cos.b2",
"电弱相变约10的负12次方秒, QCD相变约10的负5次方秒, 都是温度量级估计。粒子物理标准模型外推, 宇宙学只取量级。",
"电弱相变约10嘅负12次方秒, QCD相变约10嘅负5次方秒, 都系温度量级估计。粒子物理标准模型外推, 宇宙学只取量级。",
"電弱相転移は約10の-12乗秒、QCD相転移は約10の-5乗秒、いずれも温度の桁見積もりです。素粒子標準模型の外挿であり、宇宙論は桁のみを用います。",
"Electroweak transition at ~10^-12 s, QCD at ~10^-5 s — order-of-magnitude temperature estimates, extrapolated from the Standard Model. Cosmology uses only the scale.")
put("evo.ism.s0",
"发射星云是氢被点亮: 大质量恒星的紫外线电离周围气体, 猎户座大星云是最近的教科书原型, 梯形星团就是电灯。",
"发射星云系氢被点亮: 大质量恒星嘅紫外线电离周围气体, 猎户座大星云系最近嘅教科书原型, 梯形星团就系电灯。",
"輝線星雲は水素の発光です:大質量星の紫外線が周囲ガスを電離します。オリオン大星雲は最近の教科書的実例で、トラペジウムが電灯です。",
"Emission nebulae are hydrogen lit up: UV from massive stars ionizes the gas. The Orion Nebula is the nearest textbook case — the Trapezium is the lamp.")
put("evo.ism.s1",
"反射星云自己不发光: 昴星团的蓝色是尘埃散射, 蓝光散射更强。记住M45是偶遇尘埃, 不是诞生地 —— 发射与反射一字之差, 物理全反。",
"反射星云自己唔发光: 昴星团嘅蓝色系尘埃散射, 蓝光散射更强。记住M45系偶遇尘埃, 唔系诞生地 —— 发射同反射一字之差, 物理全反。",
"反射星雲は自ら光りません:プレアデスの青は塵の散乱で、青ほど強く散乱されます。M45は塵との偶然の遭遇であり、誕生地ではありません。",
"Reflection nebulae shine by borrowed light: the Pleiades' blue is dust scattering, stronger in blue. M45 merely met the dust — not its birthplace.")
put("evo.ism.s2",
"暗星云是剪影: 马头星云挡住身后亮星云, 煤袋是最冷的代表。暗不是空, 是尘埃太密, 光出不来。",
"暗星云系剪影: 马头星云挡住身后亮星云, 煤袋系最冻嘅代表。暗唔系空, 系尘埃太密, 光出唔到嚟。",
"暗黒星雲はシルエットです:馬頭星雲は背後の輝線星雲を遮り、コールサックは最冷の代表です。暗いのは空ではなく、塵が濃すぎて光が出られないのです。",
"Dark nebulae are silhouettes: the Horsehead blocks the bright nebula behind; the Coalsack is the coldest case. Dark means dense dust, not emptiness.")
put("evo.ism.s3",
"行星状星云是恒星的遗嘱: 指环、哑铃、螺旋, 都是中等质量恒星抛掉的外衣。中心白矮星上万度, 照亮自己吹出的泡泡, 只亮一万年。",
"行星状星云系恒星嘅遗嘱: 指环、哑铃、螺旋, 都系中等质量恒星抛走嘅外衣。中心白矮星上万度, 照亮自己吹出嘅泡泡, 只亮一万年。",
"惑星状星雲は星の遺言です:リング・ダンベル・らせんは、中質量星が脱いだ外衣です。中心の白色矮星は1万度超で、自ら吹いた泡を照らします——寿命は1万年です。",
"Planetary nebulae are a star's will: Ring, Dumbbell, Helix — envelopes shed by mid-mass stars. The white dwarf core lights its own bubble for just ten thousand years.")
put("evo.ism.s4",
"超新星遗迹是爆炸现场: 仙后座A三百年前炸的, 尘埃挡住所以当时没人看见; 面纱是两万年前的涟漪。遗迹成分直接验尸爆发机制。",
"超新星遗迹系爆炸现场: 仙后座A三百年前炸嘅, 尘埃挡住所以当时冇人睇到; 面纱系两万年前嘅涟漪。遗迹成分直接验尸爆发机制。",
"超新星残骸は爆発現場です:カシオペアAは300年前に爆発しましたが塵に遮られ目撃されませんでした;ベールは2万年前のさざ波です。残骸の組成が爆発機構を直接検証します。",
"Supernova remnants are crime scenes: Cas A exploded 300 years ago, unseen behind dust; the Veil is a 20,000-year-old ripple. Remnant composition autopsies the explosion.")
put("evo.ism.s5",
"分子云是恒星苗圃: 金牛座分子云最近, 专生小星; 银心分子环反常, 气多却生星慢。本地泡告诉我们: 我们就住在一个古老爆炸吹出的泡里。",
"分子云系恒星苗圃: 金牛座分子云最近, 专生细星; 银心分子环反常, 气多但生星慢。本地泡话俾我哋知: 我哋就住喺一个古老爆炸吹出嘅泡入面。",
"分子雲は星の苗床です:おうし座分子雲は最近で低質量星専門;銀河中心分子環は異常で、ガスは多いのに星形成は遅い。ローカルバブルが教えます:我々は古い爆発の泡の中に住んでいます。",
"Molecular clouds are nurseries: Taurus is nearest, making only small stars; the galactic-center ring is anomalous — gas-rich yet sterile. The Local Bubble tells us we live inside an ancient blast.")
put("evo.ism.s6",
"疏散星团是同龄人聚会: 昴星团一亿岁, 毕星团七亿岁正在散, 英仙座双星团一千三百万岁还带着红超巨。色星等图定年龄, 教科书例。",
"疏散星团系同龄人聚会: 昴星团一亿岁, 毕星团七亿岁散紧, 英仙座双星团一千三百万岁仲带住红超巨。色星等图定年龄, 教科书例。",
"散開星団は同級生の集まりです:プレアデスは1億歳、ヒアデスは7億歳で瓦解中、ペルセウス二重星団は1300万歳で赤色超巨星を伴います。色等級図で年齢を決める教科書例です。",
"Open clusters are classmates: Pleiades at 100 Myr, Hyades dissolving at 700 Myr, the Double Cluster at 13 Myr still with red supergiants. Ages from color-magnitude diagrams.")
put("evo.ism.s7",
"球状星团是活化石: M13约120亿岁, 半人马座欧米茄可能是被吃掉的矮星系核, 杜鹃座47藏着二十多颗毫秒脉冲星。球状星团年龄就是宇宙年龄下限。",
"球状星团系活化石: M13约120亿岁, 半人马座欧米茄可能系被食咗嘅矮星系核, 杜鹃座47收埋二十几粒毫秒脉冲星。球状星团年龄就系宇宙年龄下限。",
"球状星団は生きた化石です:M13は約120億歳、オメガ星団は飲み込まれた矮小銀河の核かもしれず、きょしちょう座47には20個超のミリ秒パルサーが潜みます。球状星団年齢は宇宙年齢の下限です。",
"Globulars are living fossils: M13 at ~12 Gyr, Omega Centauri possibly a devoured dwarf nucleus, 47 Tuc hiding 20+ millisecond pulsars. Globular ages floor the universe's age.")
put("evo.ism.s8",
"星协是刚散伙的帮派: 天蝎半人马星协最近, 心宿二就是成员。成协膨胀能定年龄, 本地泡可能就是它炸出来的。",
"星协系啱啱散伙嘅帮派: 天蝎半人马星协最近, 心宿二就系成员。成协膨胀可以定年龄, 本地泡可能就系佢炸出嚟嘅。",
"アソシエーションは解散したての集団です:さそり・ケンタウルスが最近で、アンタレスはその一員です。膨張から年齢が決まり、ローカルバブルは彼らの仕業かもしれません。",
"Associations are freshly disbanded gangs: Scorpius-Centaurus is nearest, Antares a member. Expansion dates them — and the Local Bubble may be their doing.")
put("evo.ism.s9",
"三裂星云一张图讲清三种星云: 红的是发射, 蓝的是反射, 黑缝是暗星云。分类不是背定义, 是看一张图认三种光。",
"三裂星云一张图讲清三种星云: 红嘅系发射, 蓝嘅系反射, 黑缝系暗星云。分类唔系背定义, 系睇一张图认三种光。",
"三裂星雲は1枚で3種を教えます:赤は輝線、青は反射、黒い割れ目は暗黒星雲。分類は定義の暗記ではなく、1枚の画像で3種の光を見分けることです。",
"The Trifid teaches three nebulae in one frame: red emission, blue reflection, black dark lanes. Classification is recognizing three lights, not reciting definitions.")
put("evo.ism.s10",
"巴纳德环是古老爆炸的年轮: 猎户座腰间十度大环, 超新星加星风吹出的超级泡。数星云要数泡, 泡是反馈的化石。",
"巴纳德环系古老爆炸嘅年轮: 猎户座腰间十度大环, 超新星加星风吹出嘅超级泡。数星云要数泡, 泡系反馈嘅化石。",
"バーナードループは古い爆発の年輪です:オリオンの腰の10度の環、超新星と星風の吹いたスーパー泡です。星雲を数えるなら泡を数えよ——泡はフィードバックの化石です。",
"Barnard's Loop is an ancient blast's growth ring: a ten-degree loop at Orion's waist, a superbubble of supernovae and winds. Count bubbles, not just clouds — bubbles are feedback fossils.")

json.dump(N, open(OUT, "w", encoding="utf-8"), ensure_ascii=False, indent=1)
print("part4:", len(N), "->", OUT)
