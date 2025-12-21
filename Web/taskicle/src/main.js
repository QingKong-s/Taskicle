import { createApp } from 'vue'
import App from './App.vue'
import ElementPlus from 'element-plus'
import 'element-plus/dist/index.css'
import './assets/global.css'
import router from './router'

createApp(App).use(ElementPlus).use(router).mount('#app')
