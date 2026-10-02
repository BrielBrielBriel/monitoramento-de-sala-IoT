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



// =======================
// Elementos da Página
// =======================

const temperaturaElemento = document.getElementById("temperatura");

const luminosidadeElemento = document.getElementById("luminosidade");

const ocupacaoElemento = document.getElementById("ocupacao");

const umidadeElemento = document.getElementById("umidade");

const presencaElemento = document.getElementById("presenca");

const qualidadeArElemento = document.getElementById("qualidade-ar");

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