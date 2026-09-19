# 解说词第二批: 超新星/并合/AGN/宇宙热历史 (22 段)
import json

OUT = "D:/tmp/solar-system-cpp/tools/_narr_part2.json"
N = {}
def put(i, zh, yue, ja, en):
    N[i] = {"zh": zh, "yue": yue, "ja": ja, "en": en}

put("evo.sn.s0",
"爆发开始, 光度急升。注意光谱里没有氢线 —— 这就是Ia型的分类判据, 也是它能当标准烛光的前提。",
"爆发开始, 光度急升。留意光谱入面冇氢线 —— 呢个就系Ia型嘅分类判据, 亦系佢可以当标准烛光嘅前提。",
"爆発開始、光度は急上昇します。スペクトルに水素線がないことに注意——これがIa型の分類基準であり、標準光源たり得る前提です。",
"Eruption begins, luminosity surges. Note the absence of hydrogen lines — the defining criterion of Type Ia, and what qualifies it as a standard candle.")
put("evo.sn.s1",
"约19天达到B波段极大, 绝对星等约负19.3。这是标准烛光的定标点 —— 更宽的光变对应更亮的峰值, 即宽亮关系。",
"约19日去到B波段极大, 绝对星等约负19.3。呢个系标准烛光嘅定标点 —— 光变越宽峰值越亮, 即系宽亮关系。",
"約19日でBバンド極大、絶対等級約-19.3。標準光源の較正点です——光度曲線が幅広いほど極大が明るい、幅・光度関係が成り立ちます。",
"About 19 days to B-band maximum at absolute magnitude −19.3. This anchors the standard candle — broader light curves peak brighter, the width-luminosity relation.")
put("evo.sn.s2",
"约30天, 近红外出现二次隆起 —— 铁族元素电离态变化的指纹。不是所有超新星都有, 正常Ia才有。",
"约30日, 近红外出现二次隆起 —— 铁族元素电离态变化嘅指纹。唔系粒粒超新星都有, 正常Ia先有。",
"約30日、近赤外に二度目の盛り上がり——鉄族元素の電離状態変化の指紋です。正常なIa型に特有の現象です。",
"Around day 30 a secondary bump rises in the near-infrared — the fingerprint of changing iron-group ionization, seen only in normal Type Ia.")
put("evo.sn.s3",
"60天, 过渡段。光球退入铁核, 颜色转红, 光变开始指数衰减。",
"60日, 过渡段。光球退入铁核, 颜色转红, 光变开始指数衰减。",
"60日、移行期。光球は鉄核へ後退し、色は赤くなり、光度曲線は指数関数的減衰に入ります。",
"Day 60, the transitional phase. The photosphere retreats into the iron core, colors redden, exponential decline sets in.")
put("evo.sn.s4",
"150天, 钴衰变尾。钴56衰变成铁56, 半衰期77天 —— 光变的尾巴就是元素嬗变的直接记录。",
"150日, 钴衰变尾。钴56衰变成铁56, 半衰期77日 —— 光变条尾就系元素嬗变嘅直接记录。",
"150日、コバルト減衰尾。コバルト56が鉄56に変わり、半減期77日——光度曲線の尾は元素変換の直接の記録です。",
"Day 150, the cobalt tail. Cobalt-56 decays to iron-56 with a 77-day half-life — the light curve's tail is transmutation recorded live.")
put("evo.sn.s5",
"300天后进入遗迹阶段: 抛射物稀薄成星云相, 禁线主导。数百年后, 它会成为第谷那样的超新星遗迹。",
"300日后进入遗迹阶段: 抛射物稀薄成星云相, 禁线主导。几百年后, 佢会成为第谷噉样嘅超新星遗迹。",
"300日後、残骸期に入ります:放出物は希薄な星雲相となり禁制線が支配します。数百年後、第谷のような超新星残骸になります。",
"Past day 300 the remnant phase begins — thin nebular ejecta ruled by forbidden lines. In centuries it will look like Tycho's remnant.")
put("evo.mrg.s0",
"现在: 银河系与仙女座相距约77万秒差距, 以约110公里每秒接近。仙女座的蓝移就是直接证据。",
"而家: 银河系同仙女座相距约77万秒差距, 以约110公里每秒接近。仙女座嘅蓝移就系直接证据。",
"現在:銀河系とアンドロメダは約77万パーセク離れ、秒速約110kmで接近中です。アンドロメダの青方偏移が直接の証拠です。",
"Today: the Milky Way and Andromeda lie 770 kiloparsecs apart, closing at 110 km/s. Andromeda's blueshift is the direct evidence.")
put("evo.mrg.s1",
"约40亿年后第一次近心点: 潮汐尾被拉出, 星暴触发。可以对照触须星系 —— 那就是现在进行时的并合。",
"约40亿年后第一次近心点: 潮汐尾被拉出, 星暴触发。可以对照触须星系 —— 果边就系进行紧嘅并合。",
"約40億年後に最初の近心点:潮汐尾が引き出され、スターバーストが始まります。触角銀河が現在進行形の実例です。",
"First pericenter in ~4 billion years: tidal tails drawn out, starburst ignited. The Antennae galaxies show this happening right now.")
put("evo.mrg.s2",
"约60亿年后并合: 核球合并, 盘结构被打乱, 星暴达峰后淬灭开始。银河系的旋臂就此消失。",
"约60亿年后并合: 核球合并, 盘结构被打乱, 星暴到顶后淬灭开始。银河系嘅旋臂就此消失。",
"約60億年後に合体:バルジは合わさり、円盤構造は乱され、スターバーストの後に鎮静化が始まります。銀河系の渦巻き腕は消えます。",
"Merger at ~6 billion years: bulges coalesce, disks are shredded, starburst peaks then quenches. The Milky Way's spiral arms vanish.")
put("evo.mrg.s3",
"80亿年后弛豫为大椭圆星系, 有人叫它Milkomeda。太阳系届时命运不确定 —— 大概率被甩到外晕, 但太阳本身安然无恙。",
"80亿年后弛豫成大椭圆星系, 有人叫佢Milkomeda。太阳系到时命运未定 —— 大概率被甩到外晕, 但太阳本身无恙。",
"80億年後には巨大楕円銀河に緩和します(Milkomedaの愛称も)。太陽系の運命は不確定ですが、外側ハローに放り出されつつ太陽自体は無事でしょう。",
"By 8 billion years a giant elliptical relaxes into place — Milkomeda. The solar system's fate is uncertain, likely flung to the outer halo, the Sun itself unharmed.")
put("evo.agn.s0",
"一座宁静的椭圆星系, 中心超大质量黑洞在沉睡, 没有喷流。黑洞与核球的M-西格玛关系已经就位。",
"一座宁静嘅椭圆星系, 中心超大质量黑洞瞓紧觉, 冇喷流。黑洞同核球嘅M-西格玛关系已经就位。",
"静かな楕円銀河、中心の超大質量ブラックホールは眠り、ジェットはありません。M-シグマ関係は既に成立しています。",
"A quiet elliptical galaxy, its supermassive black hole asleep, no jets. The M–sigma relation between black hole and bulge is already in place.")
put("evo.agn.s1",
"并合送来气体, 气体流入核区, 吸积盘点亮, 宽线区出现。活动星系核被触发了。",
"并合送嚟气体, 气体流入核区, 吸积盘点亮, 宽线区出现。活动星系核被触发喇。",
"合体がガスを運び、核領域へ流入して降着円盤が輝き、広輝線領域が現れます。活動銀河核が点火しました。",
"A merger delivers gas to the nucleus, the accretion disk lights up, the broad-line region appears. The active nucleus switches on.")
put("evo.agn.s2",
"相对论性喷流打通千秒差距尺度 —— M87的喷流约5千秒差距。注意: 正对喷流看, 它就是耀变体。视角决定分类, 这是统一模型的核心。",
"相对论性喷流打通千秒差距尺度 —— M87嘅喷流约5千秒差距。留意: 正对喷流睇, 佢就系耀变体。视角决定分类, 呢个系统一模型嘅核心。",
"相対論的ジェットがキロパーセク級を貫きます(M87で約5kpc)。正面から見ればブレーザーです。見かけの分類を決めるのは視角——統一モデルの核心です。",
"Relativistic jets punch through kiloparsec scales — M87's spans ~5 kpc. Seen head-on, this same object is a blazar. Viewing angle decides classification: the heart of the unified model.")
put("evo.agn.s3",
"喷流瓣长到数百千秒差距, 激波加热星系周气体 —— 这就是AGN反馈, 可能是星系停止形成恒星的原因之一。注意: 候选机制, 不是定论。",
"喷流瓣长到数百千秒差距, 激波加热星系周气体 —— 呢个就系AGN反馈, 可能系星系停造星嘅原因之一。留意: 候选机制, 唔系定论。",
"ローブは数百kpcに成長し、衝撃波が銀河周囲ガスを加熱します——AGNフィードバック、星形成停止の候補メカニズムです(未確定)。",
"Lobes spanning hundreds of kiloparsecs shock-heat circumgalactic gas — AGN feedback, a candidate cause of quenching. Candidate, not verdict.")
put("evo.agn.s4",
"燃料耗尽, 喷流熄灭, 瓣辐射老化变陡。只剩遗迹瓣在射电波段慢慢变暗 —— 巨椭圆星系重归宁静。",
"燃料烧完, 喷流熄灭, 瓣辐射老化变陡。净返遗迹瓣喺射电波段慢慢变暗 —— 巨椭圆星系重归宁静。",
"燃料が尽き、ジェットは消え、ローブの放射は老化します。残骸ローブが電波で静かに減衰します——巨大楕円銀河は静けさに戻ります。",
"Fuel exhausted, jets die, lobes fade and steepen. Only relic lobes linger in radio — the giant elliptical returns to quiet.")
put("evo.cos.s0",
"大爆炸后1秒到3分钟: 太初核合成。氦4质量分数约25%, 氘氢比锁定重子密度 —— 与宇宙微波背景独立一致, 大爆炸最硬的预言之一。",
"大爆炸后1秒到3分钟: 太初核合成。氦4质量分数约25%, 氘氢比锁定重子密度 —— 同宇宙微波背景独立一致, 大爆炸最硬嘅预言之一。",
"ビッグバン後1秒〜3分:元素合成。ヘリウム4質量分率約25%、重水素比がバリオン密度を決めます——CMBと独立に一致する、大爆炸論最強の予言です。",
"One second to three minutes after the Bang: primordial nucleosynthesis. Helium-4 at ~25%, deuterium fixing baryon density — matching the CMB independently, the Big Bang's hardest prediction.")
put("evo.cos.s1",
"约5万年, 物质密度超过辐射: 物质-辐射相等。从此引力战胜辐射压, 扰动开始增长 —— 结构形成的起点。",
"约5万年, 物质密度超过辐射: 物质-辐射相等。由呢刻起引力战胜辐射压, 扰动开始增长 —— 结构形成嘅起点。",
"約5万年、物質密度が放射を上回ります:物質・放射等価。以後、重力が放射圧に勝ち、揺らぎが成長します——構造形成の起点です。",
"At ~50,000 years matter overtakes radiation. Gravity beats radiation pressure from here on, perturbations grow — the starting line of structure formation.")
put("evo.cos.s2",
"38万年, 复合: 电子与质子结合, 光子退耦自由传播 —— 这就是今天2.7255开的宇宙微波背景, 红移约1090。我们能看到的最古老的光。",
"38万年, 复合: 电子同质子结合, 光子退耦自由传播 —— 呢个就系今日2.7255度嘅宇宙微波背景, 红移约1090。我哋睇到最古老嘅光。",
"38万年、再結合:電子と陽子が結びつき、光子は自由に進みます——今日2.7255度の宇宙背景放射、赤方偏移約1090。我々が見られる最古の光です。",
"380,000 years, recombination: electrons meet protons, photons stream free — today's 2.7255 K cosmic microwave background at redshift ~1090. The oldest light we can ever see.")
put("evo.cos.s3",
"约2亿年, 第一代恒星点亮 —— 第三星族, 理论预言为主, 实测尚未确认。黑暗时代结束。",
"约2亿年, 第一代恒星点亮 —— 第三星族, 理论预言为主, 实测仲未确认。黑暗时代结束。",
"約2億年、最初の星々が灯ります——種族III、主に理論予言で、観測は未確定です。暗黒時代の終わりです。",
"Around 200 million years the first stars ignite — Population III, still mostly theoretical, unconfirmed observationally. The dark ages end.")
put("evo.cos.s4",
"红移6到10, 再电离: 星系的紫外光子电离星系际氢。类星体光谱中的 Gunn-Peterson 谷就是证据。",
"红移6到10, 再电离: 星系嘅紫外光子电离星系际氢。类星体光谱入面嘅 Gunn-Peterson 谷就系证据。",
"赤方偏移6〜10、再電離:銀河の紫外線が銀河間水素を電離します。クエーサースペクトルのGunn-Peterson谷が証拠です。",
"Redshift 6 to 10, reionization: galactic ultraviolet ionizes intergalactic hydrogen. The Gunn–Peterson trough in quasar spectra is the evidence.")
put("evo.cos.s5",
"红移约0.6, 暗能量接管, 膨胀开始加速。1998年超新星证据; 本项目的哈勃图面板用Δ卡方实测复现了这一结论。",
"红移约0.6, 暗能量接管, 膨胀开始加速。1998年超新星证据; 本项目嘅哈勃图面板用Δ卡方实测复现咗呢个结论。",
"赤方偏移約0.6、ダークエネルギーが支配し、膨張は加速に転じます。1998年超新星の証拠;本プロジェクトのハッブル図もΔχ²で再現しています。",
"At redshift ~0.6 dark energy takes over and expansion accelerates. The 1998 supernova evidence — reproduced in this project's Hubble panel via Δχ².")
put("evo.cos.s6",
"138亿年后的现在。普朗克2018: 哈勃常数67.4, 物质密度0.315, 平直宇宙。但普朗克与距离阶梯的哈勃常数差约5个标准差 —— 当前最大未解。",
"138亿年后嘅而家。普朗克2018: 哈勃常数67.4, 物质密度0.315, 平直宇宙。但普朗克同距离阶梯嘅哈勃常数差约5个标准差 —— 当前最大未解。",
"138億年後の現在。Planck2018:ハッブル定数67.4、物質密度0.315、平坦宇宙。しかしCMBと距離梯子の値は約5σ食い違い——目下最大の未解決問題です。",
"Today, 13.8 billion years on. Planck 2018: H₀ = 67.4, Ωm = 0.315, a flat universe. Yet CMB and distance-ladder H₀ differ by ~5σ — the biggest open problem in cosmology.")

json.dump(N, open(OUT, "w", encoding="utf-8"), ensure_ascii=False, indent=1)
print("part2:", len(N), "->", OUT)
