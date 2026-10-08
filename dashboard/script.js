/*Importar firebase*/
import { initializeApp } from "https://www.gstatic.com/firebasejs/10.8.0/firebase-app.js";
import { getDatabase, ref, set, onValue } from "https://www.gstatic.com/firebasejs/10.8.0/firebase-database.js";

/*Config firebase*/
const firebaseConfig = {
  apiKey: "AIzaSyAPi-HXO7ZFhItHS4KiEB-Dgrob-wk1BCc",
  authDomain: "senai-led-id.firebaseapp.com",
  databaseURL: "https://senai-led-id-default-rtdb.firebaseio.com",
  projectId: "senai-led-id",
  storageBucket: "senai-led-id.firebasestorage.app",
  messagingSenderId: "766377806899",
  appId: "1:766377806899:web:a716d1a9bfe2fba873a746"
};

/* Initialize firebase*/
const app = initializeApp(firebaseConfig);

const database = getDatabase(app);

// Referências do banco

const sensoresRef = ref(
    database,
    "dispositivos/esp32_01/sensores"
);

const classificacoesRef = ref (
    database,
    "dispositivos/esp32_01/classificacoes"
);

const conexaoRef = ref(
    database,
    ".info/connected"
);

// =======================
// Elementos da Página
// =======================

const temperaturaElemento = document.getElementById("temperatura");

const luminosidadeElemento = document.getElementById("luminosidade");

const ocupacaoElemento = document.getElementById("ocupacao");

const umidadeElemento = document.getElementById("umidade");

const presencaElemento = document.getElementById("presenca");

const qualidadeArElemento = document.getElementById("mq2");


const icsElemento = document.getElementById("ics");
const classificapIcsElemento = document.getElementById("ics-estado");
const icsAreaElemento = document.getElementById("ics-area");

const statusTemperatura = document.getElementById("status-temperatura");
const statusUmidade = document.getElementById("status-umidade");
const statusLuminosidade = document.getElementById("status-luminosidade");
const statusOcupacao = document.getElementById("status-ocupacao");
const statusQualidadeDoAr = document.getElementById("status-mq2");
const statusGeral = document.getElementById("status");

let statusGeralFundo = document.getElementById("status-geral");

let statusConexao = document.getElementById("status-conexao");
let indicadorStatus = document.getElementById("indicador-status");
// -----------------------------------------------------
// MONITORA CONEXÃO COM FIREBASE
// -----------------------------------------------------

onValue(
    conexaoRef,

    (snapshot) => {

        const conectado =
            snapshot.val();


        if (conectado) {

            statusConexao.innerText =
                "Firebase conectado";

            indicadorStatus.style.background =
                "#2ecc71";

        }

        else {

            statusConexao.innerText =
                "Firebase desconectado";

            indicadorStatus.style.background =
                "#e74c3c";

        }

    }
);

// -----------------------------------------------------
// RECEBE DADOS DOS SENSORES
// -----------------------------------------------------


onValue(
    sensoresRef,

    (snapshot) => {

        const dados =
            snapshot.val();


        if (!dados) {

            return;

        }

        // TEMPERATURA

        if (dados.temperatura !== undefined) {

            temperaturaElemento.innerText =
                Number(
                    dados.temperatura
                ).toFixed(1);

        }

        // LUMINOSIDADE

        if (dados.luminosidade !== undefined) {

            luminosidadeElemento.innerText = 
            Number(
                dados.luminosidade
            ).toFixed(1);
        }

        // OCUPACAO

        if (dados.ocupacao !== undefined) {
            ocupacaoElemento.innerText =
            Number(
                dados.ocupacao
            ).toFixed(1);
        }

        // UMIDADE

        if (dados.umidade !== undefined) {
            umidadeElemento.innerText =
            Number(
                dados.umidade
            ).toFixed(1);
        }

        // PRESENÇA

        if (dados.presenca !== undefined) {
            if (dados.presenca !== false) {
                presencaElemento.innerText = "DETECTADO";
            } else {
                presencaElemento.innerText = "NÃO DETECTADO";
            }
        }

        // QUALIDADE DO AR

        if (dados.mq2 !== undefined) {
            if (dados.mq2 < 3665) {
                qualidadeArElemento.innerText = "NORMAL";
            } else if (dados.mq2 >= 3665 && dados.mq2 <= 3762) {
                qualidadeArElemento.innerText = "REGULAR";
            } else {
                qualidadeArElemento.innerText = "RUIM";
            }
        }

    }
);

onValue(
    classificacoesRef,

    (snapshot) => {

        const dados =
            snapshot.val();


        if (!dados) {

            return;

        }

        // ICS

        if (dados.ics !== undefined) {
            icsElemento.innerText = 
            Number(
                dados.ics
            ).toFixed(1);
        }

        // CLASSIFICAÇÃO ICS

        if (dados.classificacao_ics !== undefined) {
            classificapIcsElemento.innerText = 
            String(
                dados.classificacao_ics
            );
        }

        if (dados.classificacao_ics === "RUIM") {
            icsAreaElemento.style.backgroundColor = "red"
        } else if (dados.classificacao_ics === "MEDIANA") {
            icsAreaElemento.style.backgroundColor = "orange"
        } else {
            icsAreaElemento.style.backgroundColor = "greenyellow"
        }

        // =================
        // STATUS DA SALA
        // =================

        // TEMPERATURA

        if (dados.temperatura !== undefined) {
            statusTemperatura.innerText = 
            String(
                dados.temperatura
            );
        }

        // UMIDADE*

        if (dados.umidade !== undefined) {
            statusUmidade.innerText = 
            String(
                dados.umidade
            );
        }

        // LUMINOSIDADE

        if (dados.luminosidade !== undefined) {
            statusLuminosidade.innerText = 
            String(
                dados.luminosidade
            );
        }

        // OCUPAÇÃO

        if (dados.ocupacao !== undefined) {
            statusOcupacao.innerText = 
            String(
                dados.ocupacao
            );
        }

        // QUALIDADE DO AR

        if (dados.qualidade_do_ar !== undefined) {
            statusQualidadeDoAr.innerText =
            String(
                dados.qualidade_do_ar
            );
        }

        // STATUS GERAL

        if (dados.classificacao_ics) {
            statusGeral.innerText = 
            String(
                dados.classificacao_ics
            );
        }

        if (dados.classificacao_ics === "RUIM") {
            statusGeralFundo.style.backgroundColor = "red"
        } else if (dados.classificacao_ics === "MEDIANA") {
            statusGeralFundo.style.backgroundColor = "orange"
        } else {
            statusGeralFundo.style.backgroundColor = "greenyellow"
        }
    }
);

// RELACIONADO AO GRÁFICO

const historicoTempRef = ref (
    database,
    "dispositivos/esp32_01/historico_temp"
);

const historicoLumRef = ref (
    database,
    "dispositivos/esp32_01/historico_lum"
);

const historicoOcupRef = ref (
    database,
    "dispositivos/esp32_01/historico_ocup"
);

const historicoUmidadeRef = ref (
    database,
    "dispositivos/esp32_01/historico_umidade"
);

const max_grafico = 10;

const dados_graficos = {
    temperatura: [],
    luminosidade: [],
    ocupacao: [],
    umidade: []
};

//MONITORAMENTOS
// TEMPERATURA

onValue(historicoTempRef, (snapshot) => {
    const dados = snapshot.val();
    console.log(dados);
    if (!dados) {
        return;
    }
    carregarEntradas("temperatura", dados);
});


// LUMINOSIDADE
onValue(historicoLumRef, (snapshot) => {
    const dados = snapshot.val();
    if (!dados) {
        return;
    }
    carregarEntradas("luminosidade", dados);
});

// OCUPACAO
onValue(historicoOcupRef, (snapshot) => {
    const dados = snapshot.val();
    if (!dados) {
        return;
    }
    carregarEntradas("ocupacao", dados);
});

// UMIDADE
onValue(historicoUmidadeRef, (snapshot) => {
    const dados = snapshot.val();
    if (!dados) {
        return;
    }
    carregarEntradas("umidade", dados);
});

function carregarEntradas(nome, dados) {
    const ultimasEntradas = obterUltimasEntradas(dados, max_grafico); 
    dados_graficos[nome] = mapearEntradas(ultimasEntradas);
    pesquisarRenderizarGraficoDe(nome);
}

function pesquisarRenderizarGraficoDe(nome) {
    let grafico = document.querySelector("#grafico-" + nome);
    renderGrafico(grafico, dados_graficos[nome], nome);
}

function obterUltimasEntradas(dados, quantidade) {
    const ultimas = Object.values(dados)
        .sort((a, b) => a.timestamp - b.timestamp)
        .slice(-quantidade);

    return Array.from({ length: quantidade }, (_, i) => ultimas[i]);
}

function mapearEntradas(entradas) {
    return entradas.map(item => ({
        timestamp: item?.timestamp === undefined ? "---" : formatarTimestamp(item.timestamp),
        valor: item?.valor === undefined ? 0 : item.valor
    }));
}

/*
dados_graficos.temperatura[0].horario = "13:23";
dados_graficos.temperatura[0].valor = 27;

dados_graficos.temperatura[1].horario = "14:23";
dados_graficos.temperatura[1].valor = 28;

dados_graficos.temperatura[2].horario = "15:23";
dados_graficos.temperatura[2].valor = 26;

dados_graficos.temperatura[3].horario = "16:23";
dados_graficos.temperatura[3].valor = 24;

dados_graficos.temperatura[4].horario = "17:23";
dados_graficos.temperatura[4].valor = 22;
*/

function obterUnidade(tipo) {
    if(tipo === "temperatura") 
        return "C°";
    
    if(tipo === "luminosidade") {
        return "lux";
    }

    if(tipo === "ocupacao" || tipo === "umidade") {
        return "%";
    }
}

function formatarTimestamp(timestamp) {
    const data = new Date(timestamp);
    const dia = String(data.getDate()).padStart(2, "0");
    const mes = String(data.getMonth() + 1).padStart(2, "0");
    const horas = String(data.getHours()).padStart(2, "0");
    const minutos = String(data.getMinutes()).padStart(2, "0");

    return `${dia}/${mes} ${horas}:${minutos}`;
}

function renderGrafico(elemento, dados, tipo) {
    console.log(dados)

    const unidade = obterUnidade(tipo);

    const tbody = elemento.querySelector("tbody");

    const valores = dados.map(item => item.valor);

    const maiorValor = Math.max(...valores);

    tbody.innerHTML = dados.map(item => {
        const tamanho = maiorValor > 0 ? item.valor / maiorValor: 0;
        let valor = item.valor < 0 ? " " : item.valor + unidade;
        return `
            <tr>
                <th scope="row">${item.timestamp}</th>
                <td style="--size: ${tamanho}; padding-top: 5px;">
                    ${valor}
                </td>
            </tr>
        `;

    }).join("");

    atualizarMetricas(elemento, valores);
}

// ATUALIZAR PRA SUPORTAR TEMPERATURA NEGATIVA?

function atualizarMetricas(elemento, valores) {
    let valoresFitrados = valores.filter(num => num >= 0);

    let maximo = Math.max(...valoresFitrados);
    let minimo = Math.min(...valoresFitrados);

    const quantidade = valoresFitrados.length;
    let total = valoresFitrados.reduce((acc, num) => acc + num, 0);

    const media = total / quantidade;
    atualizarInterfaceMetricas(elemento, maximo, minimo, media)
}

function atualizarInterfaceMetricas(elemento, maximo, minimo, media) {
    elemento.querySelector("#media").innerText = "Média: " + media;
    elemento.querySelector("#minimo").innerText = "Mínimo: " + minimo;
    elemento.querySelector("#maximo").innerText = "Máximo: " + maximo;
}