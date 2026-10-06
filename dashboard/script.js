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