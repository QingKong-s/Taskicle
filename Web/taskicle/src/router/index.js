import { createRouter, createWebHistory } from 'vue-router'
import ArticlePage from '@/components/ArticlePage.vue'
import TaskPage from '@/components/TaskPage.vue'
import SettingPage from '@/components/SettingPage.vue'

const routes = [
  { path: '/index.html', redirect: '/' },
  { path: '/', redirect: '/articles' },
  { path: '/articles', name: 'articles', component: ArticlePage },
  { path: '/settings', name: 'settings', component: SettingPage },
  { path: '/tasks', name: 'tasks', component: TaskPage },
]

const router = createRouter({
  history: createWebHistory(),
  routes,
})

export default router
