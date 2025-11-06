import subprocess, re, os, shutil
import pandas as pd
import matplotlib.pyplot as plt
import time 


NOME_PROGRAMA_PART1 = "lab2-part1" 
NOME_PROGRAMA_PART2 = "lab2-part2"
COMANDO_NS3 = "./ns3"
DIR_SAIDA = "Lab2_Sobrenome_Nome/Part1"
DIR_SCRATCH = "scratch"
ARQ_TRACE_FIXO = "Congestion_Control-cwnd.data"
CAMINHO_TRACE_FIXO_1 = os.path.join(DIR_SCRATCH, "resultados", ARQ_TRACE_FIXO)

def prepara_dir():
    if os.path.exists("Lab2_Sobrenome_Nome"): shutil.rmtree("Lab2_Sobrenome_Nome")
    os.makedirs(os.path.join("Lab2_Sobrenome_Nome", 'Part1', 'plots'), exist_ok=True)
    os.makedirs(os.path.join("Lab2_Sobrenome_Nome", 'Part2', 'plots'), exist_ok=True)

def roda_simulacao(nome_executavel, parametros):
    lista_args = [f"--{k}={v}" for k, v in parametros.items()]
    cmd_args = f'{nome_executavel} {" ".join(lista_args)}'
    cmd = [COMANDO_NS3, "run", cmd_args]
    print(f"\nRodando: {parametros.get('transport_prot', 'N/A')} | {parametros.get('nFlows', 0)} flows | Args: {cmd_args}")
    try:
        resultado = subprocess.run(cmd, capture_output=True, text=True, check=True)
        saida = resultado.stdout
        
        match_agg = re.search(r"Total Aggregate Goodput: ([\d\.e\+]+) bps", saida)
        match_avg_d1 = re.search(r"Dest 1 \(Fast RTT\) \| Average Per-Flow Goodput: ([\d\.e\+]+) bps", saida)
        match_avg_d2 = re.search(r"Dest 2 \(Slow RTT\) \| Average Per-Flow Goodput: ([\d\.e\+]+) bps", saida)

        if match_agg and match_avg_d1 and match_avg_d2: 
            return {
                'goodput_agg': float(match_agg.group(1)),
                'goodput_avg_d1': float(match_avg_d1.group(1)),
                'goodput_avg_d2': float(match_avg_d2.group(1)),
                'saida': saida
            }
        
        match_agg_part1 = re.search(r"Goodput Agregado Total: ([\d\.e\+]+) bps", saida)
        if match_agg_part1:
            return {'goodput_agg': float(match_agg_part1.group(1)), 'saida': saida}
            
        print(f"Aviso: Goodput não encontrado para {parametros}")
        return {'goodput_agg': None, 'saida': saida}

    except subprocess.CalledProcessError as e:
        print(f"ERRO NS3 ({e.returncode}):\nComando: {' '.join(cmd)}\nStderr: {e.stderr}")
        return {'goodput_agg': None, 'saida': ""}
    except FileNotFoundError:
        print(f"ERRO: Comando '{COMANDO_NS3}' não achado.")
        return {'goodput_agg': None, 'saida': ""}

def move_trace(dir_base, nome_parte, prot):
    pasta_destino = os.path.join(dir_base, nome_parte, prot)
    os.makedirs(pasta_destino, exist_ok=True)
    src = CAMINHO_TRACE_FIXO_1
    dst = os.path.join(pasta_destino, ARQ_TRACE_FIXO)
    if os.path.exists(src):
        shutil.move(src, dst)
        print(f"Trace CWND movido para {dst}")
        return dst
    else:
        return None

def le_cwnd(caminho):
    dados = pd.read_csv(caminho, sep='\s+', header=None, names=['Tempo', 'Cwnd'])
    return dados['Tempo'].tolist(), dados['Cwnd'].tolist()


def parte_1a():
    print("Iniciando Parte 1a")
    cfg1a = { 'dataRate': "10Mbps", 'delay': "100ms", 'errorRate': 0.00001, 'nFlows': 1, 'seed': 1, 'transport_prot': "" }
    resultados = {}
    dir_base = "Lab2_Sobrenome_Nome"

    prot_c = "TcpCubic"
    params_c = cfg1a.copy(); params_c['transport_prot'] = prot_c
    res_c = roda_simulacao(NOME_PROGRAMA_PART1, params_c)
    resultados[prot_c] = res_c['goodput_agg']
    caminho_c = move_trace(dir_base, 'Part1', prot_c)

    prot_r = "TcpNewReno"
    params_r = cfg1a.copy(); params_r['transport_prot'] = prot_r
    res_r = roda_simulacao(NOME_PROGRAMA_PART1, params_r)
    resultados[prot_r] = res_r['goodput_agg']
    caminho_r = move_trace(dir_base, 'Part1', prot_r)

    if caminho_c and caminho_r:
        try:
            t_c, cw_c = le_cwnd(caminho_c)
            t_r, cw_r = le_cwnd(caminho_r)

            plt.figure(figsize=(12, 6))
            plt.step(t_c, cw_c, where='post', label='TCP CUBIC')
            plt.step(t_r, cw_r, where='post', label='TCP NewReno')
            plt.title(f"Cwnd vs Tempo - 1 Flow (10Mbps, 100ms, 1e-5)")
            plt.xlabel("Tempo (s)"); plt.ylabel("Janela de Congestionamento (Bytes)")
            plt.grid(True); plt.legend()
            caminho_saida = os.path.join(dir_base, 'Part1', 'plots', "Part1a_CWND_Comparison.png")
            plt.savefig(caminho_saida); plt.close()
            print(f"Gráfico CWND salvo em: {caminho_saida}")
        except Exception as e:
            print(f"Erro ao ler CWND: {e}")
    
    cfg4f = {'dataRate': "10Mbps", 'delay': "100ms", 'errorRate': 0.00001, 'nFlows': 4, 'seed': 1}
    dir_p1 = os.path.join(dir_base, 'Part1')
    for prot_ex in ["TcpCubic", "TcpNewReno"]:
        params_4f = cfg4f.copy(); params_4f['transport_prot'] = prot_ex
        print(f"Rodando amostra (4 flows, 1a config): {prot_ex}")
        res_4f = roda_simulacao(NOME_PROGRAMA_PART1, params_4f)
        nome_saida = f'Part1a_SampleOutput_4Flows_{prot_ex}.txt'
        with open(os.path.join(dir_p1, nome_saida), 'w') as f:
            f.write(res_4f['saida'])
    
    print("Parte 1a OK.")
    return resultados, cfg1a

def parte_1b():
    print("Iniciando Parte 1b")
    cfg_fixa = {'dataRate': "1Mbps", 'errorRate': 0.00001, 'seed': 2}
    delays = ["50ms", "100ms", "150ms", "200ms", "250ms", "300ms"]
    n_flows = [1, 2, 4]; protocolos = ["TcpCubic", "TcpNewReno"]
    dados = []
    
    for prot in protocolos:
        for n in n_flows:
            for d in delays:
                params = cfg_fixa.copy()
                params.update({'transport_prot': prot, 'nFlows': n, 'delay': d})
                
                res = roda_simulacao(NOME_PROGRAMA_PART1, params) 
                goodput_agg = res['goodput_agg']
                
                d_ms = int(d.replace("ms", ""))
                dados.append({
                    'Delay': d_ms, 'NFlows': n, 'Protocol': prot,
                    'Goodput': (goodput_agg / 1e6) if goodput_agg is not None else 0
                })
                
    df1b = pd.DataFrame(dados)
    plt.figure(figsize=(10, 7))
    for (prot, n), grupo in df1b.groupby(['Protocol', 'NFlows']):
        label = f"{prot} ({n} Flow{'s' if n > 1 else ''})"
        plt.plot(grupo['Delay'], grupo['Goodput'], marker='o', linestyle='-', label=label)
        
    plt.title("Goodput vs. Latência (1 Mbps, Error Rate 1e-5)")
    plt.xlabel("Delay (ms)"); plt.ylabel("Goodput Agregado (Mbps)")
    plt.ylim(bottom=0); plt.grid(True); plt.legend()
    caminho_saida = os.path.join("Lab2_Sobrenome_Nome", 'Part1', 'plots', "Part1b_Goodput_vs_Delay.png")
    plt.savefig(caminho_saida); plt.close()
    print(f"Gráfico 1b salvo.")
    return df1b

def parte_1c():
    print("Iniciando Parte 1c")
    cfg_fixa = {'dataRate': "1Mbps", 'delay': "1ms", 'seed': 3}
    erros = [0.00001, 0.00005, 0.0001, 0.0005, 0.001]
    n_flows = [1, 2, 4]; protocolos = ["TcpCubic", "TcpNewReno"]
    dados = []
    
    for prot in protocolos:
        for n in n_flows:
            for erro in erros:
                params = cfg_fixa.copy()
                params.update({'transport_prot': prot, 'nFlows': n, 'errorRate': erro})
                
                res = roda_simulacao(NOME_PROGRAMA_PART1, params)
                goodput_agg = res['goodput_agg']
                
                dados.append({
                    'ErrorRate': erro, 'NFlows': n, 'Protocol': prot,
                    'Goodput': (goodput_agg / 1e6) if goodput_agg is not None else 0
                })
                
    df1c = pd.DataFrame(dados)
    plt.figure(figsize=(10, 7))
    for (prot, n), grupo in df1c.groupby(['Protocol', 'NFlows']):
        label = f"{prot} ({n} Flows)"
        plt.plot(grupo['ErrorRate'], grupo['Goodput'], marker='o', linestyle='-', label=label)
        
    plt.title("Goodput vs. Taxa de Erro (1 Mbps, Delay: 1 ms)")
    plt.xlabel("Taxa de Erro (ErrorRate)"); plt.ylabel("Goodput Agregado (Mbps)")
    plt.xscale('log'); plt.ylim(bottom=0); plt.grid(True, which="both", ls="--"); plt.legend()
    caminho_saida = os.path.join("Lab2_Sobrenome_Nome", 'Part1', 'plots', "Part1c_Goodput_vs_ErrorRate.png")
    plt.savefig(caminho_saida); plt.close()
    print(f"Gráfico 1c salvo.")
    return df1c

def executa_2():
    print("\nIniciando Parte 2: Topologia Heterogênea")
    
    cfg_fixa = {
        'dataRate': "1Mbps", 
        'delay': "20ms", 
        'errorRate': 0.00001, 
        'seed': 8080 
    }
    
    n_runs = 10 
    n_flows = [2, 4, 6, 8]
    protocolos = ["TcpCubic", "TcpNewReno"]
    
    dados_totais = []
    dir_base = "Lab2_Sobrenome_Nome"
    dir_p2 = os.path.join(dir_base, 'Part2')
    
    for prot in protocolos:
        for n in n_flows:
            goodputs_d1 = []
            goodputs_d2 = []
            
            for run in range(n_runs):
                seed_atual = cfg_fixa['seed'] + run
                
                params = cfg_fixa.copy()
                params.update({'transport_prot': prot, 'nFlows': n, 'seed': seed_atual})

                res = roda_simulacao(NOME_PROGRAMA_PART2, params)
                
                if res['goodput_avg_d1'] is not None and res['goodput_avg_d2'] is not None:
                    goodputs_d1.append(res['goodput_avg_d1'])
                    goodputs_d2.append(res['goodput_avg_d2'])
                
                if n == 4 and run == 0:
                    nome_saida = f'Part2_SampleOutput_4Flows_{prot}.txt'
                    with open(os.path.join(dir_p2, nome_saida), 'w') as f:
                        f.write(res['saida'])
                        
            if goodputs_d1 and goodputs_d2:
                avg_d1 = sum(goodputs_d1) / n_runs
                avg_d2 = sum(goodputs_d2) / n_runs
                
                dados_totais.append({
                    'Protocol': prot, 'NFlows': n, 'Dest': 'Dest1 (Fast RTT)', 
                    'Goodput_Avg': avg_d1 / 1e6
                })
                dados_totais.append({
                    'Protocol': prot, 'NFlows': n, 'Dest': 'Dest2 (Slow RTT)', 
                    'Goodput_Avg': avg_d2 / 1e6
                })
            else:
                print(f"AVISO: Não foram obtidos dados de goodput para {prot}, {n} flows.")

    df2 = pd.DataFrame(dados_totais)
    
    plt.figure(figsize=(10, 7))
    
    for (prot, dest), grupo in df2.groupby(['Protocol', 'Dest']):
        label = f"{prot} - {dest}"
        plt.plot(grupo['NFlows'], grupo['Goodput_Avg'], marker='o', linestyle='-', label=label)
        
    plt.title("Goodput Médio por Flow vs. Número de Flows")
    plt.xlabel("Número de Flows (nFlows)"); plt.ylabel("Goodput Médio por Flow (Mbps)")
    plt.xticks(n_flows); plt.ylim(bottom=0); plt.grid(True); plt.legend()
    
    caminho_saifa = os.path.join(dir_p2, 'plots', "Part2_Goodput_vs_nFlows.png")
    plt.savefig(caminho_saifa); plt.close()
    print(f"Gráfico Parte 2 salvo em: {caminho_saifa}")
    
    print("Parte 2 OK.")
    return df2


if not os.path.exists("Lab2_Sobrenome_Nome"):
    print("Criando diretório Lab2_Sobrenome_Nome...")
prepara_dir()

r_1a, cfg_1a = parte_1a()
df_1b = parte_1b()
df_1c = parte_1c()

print("\n--- Goodput 1a (1 Flow) ---")
print(f"CUBIC: {r_1a.get('TcpCubic'):.2f} bps")
print(f"RENO: {r_1a.get('TcpNewReno'):.2f} bps")

df_2 = executa_2()

print("\nExecução de todas as partes concluída.")