<template>
  <div class="main-page">
    <div class="sidebar" :style="{ width: leftWidth + 'px' }">
      <div class="sidebar-header"></div>
      <el-menu class="menu" default-active="articles" :default-openeds="['tasks', 'articles']"
        background-color="#1f2937" text-color="#ffffff" @select="onMenuSelect">
        <el-sub-menu index="articles">
          <template #title>
            <el-icon>
              <Notebook />
            </el-icon><span>文章</span>
          </template>

          <div class="menu-footer">
            <el-button type="text" icon="plus" @click="onAddPageGroup">+ 新建页面组</el-button>
          </div>

          <el-menu-item v-for="g in pageGroups" :key="g.page_group_id" :index="`articles:group:${g.page_group_id}`"
            :class="{ 'menu-item-active': isActive('articles', 'group', g.page_group_id) }">
            <div class="menu-item-row">
              <div class="menu-item-name">{{ g.group_name }}</div>
              <div class="menu-item-actions">
                <el-button class="edit-btn" type="text" size="small" @click.stop="onEditPageGroup(g)">
                  <el-icon>
                    <Edit />
                  </el-icon>
                </el-button>
                <el-button class="edit-btn" type="text" size="small" @click.stop="onDeletePageGroup(g)">
                  <el-icon>
                    <Delete />
                  </el-icon>
                </el-button>
              </div>
            </div>
          </el-menu-item>
        </el-sub-menu>

        <el-sub-menu index="tasks">
          <template #title>
            <el-icon>
              <DocumentChecked />
            </el-icon><span>任务</span>
          </template>

          <div class="menu-footer">
            <el-button type="text" icon="plus" @click="onAddProject">+ 新建项目</el-button>
          </div>

          <el-menu-item v-for="p in projects" :key="p.project_id" :index="`tasks:project:${p.project_id}`"
            :class="{ 'menu-item-active': isActive('tasks', 'project', p.project_id) }">
            <div class="menu-item-row">
              <div class="menu-item-name">{{ p.project_name }}</div>
              <div class="menu-item-actions">
                <el-button class="edit-btn" type="text" size="small" @click.stop="onEditProject(p)">
                  <el-icon>
                    <Edit />
                  </el-icon>
                </el-button>
                <el-button class="edit-btn" type="text" size="small" @click.stop="onDeleteProject(p)">
                  <el-icon>
                    <Delete />
                  </el-icon>
                </el-button>
              </div>
            </div>
          </el-menu-item>
        </el-sub-menu>
      </el-menu>

      <el-menu class="sidebar-footer" background-color="#1f2937" text-color="#ffffff">
        <el-menu-item class="menu-item-row">
          <el-button type="text" @click="onSettings">
            <el-icon>
              <Setting />
            </el-icon>
            <span class="menu-item-name">设置</span>
          </el-button>
        </el-menu-item>

        <template v-if="!loggedIn">
          <el-menu-item class="menu-item-row">
            <el-button type="text" @click="openLoginDialog('login')">
              <el-icon>
                <User />
              </el-icon>
              <span class="menu-item-name">登录</span>
            </el-button>
          </el-menu-item>
        </template>
        <template v-else>
          <el-dropdown trigger="click" style="width: 100%;">
            <el-menu-item class="menu-item-row">
              <el-button type="text">
                <el-icon>
                  <User />
                </el-icon>
                <span class="menu-item-name">{{ username }}</span>
              </el-button>
            </el-menu-item>
            <template #dropdown>
              <el-dropdown-menu>
                <el-dropdown-item @click.prevent="openLoginDialog('login')">登录另一账户</el-dropdown-item>
                <el-dropdown-item disabled>退出登录</el-dropdown-item>
              </el-dropdown-menu>
            </template>
          </el-dropdown>
        </template>
      </el-menu>
    </div>
    <Splitter @ondrag="onDrag" />
    <div class="content">
      <router-view />

      <el-dialog v-model="loginDialogVisible" width="420px" :close-on-click-modal="false">
        <template #title>
          <span v-if="loginMode === 'login'">登录</span>
          <span v-else>注册</span>
        </template>
        <div v-if="loginMode === 'login'">
          <el-form label-position="top">
            <el-form-item label="用户名">
              <el-input v-model="loginForm.user_name" autocomplete="off" />
            </el-form-item>
            <el-form-item label="密码">
              <el-input v-model="loginForm.password" type="password" autocomplete="off" />
            </el-form-item>
          </el-form>
          <div style="text-align:right; margin-top:12px">
            <el-button @click="loginDialogVisible = false">取消</el-button>
            <el-button type="primary" @click="doLogin">登录</el-button>
          </div>
        </div>
        <div v-else>
          <el-form label-position="top">
            <el-form-item label="用户名">
              <el-input v-model="registerForm.name" autocomplete="off" />
            </el-form-item>
            <el-form-item label="密码">
              <el-input v-model="registerForm.password" type="password" autocomplete="off" />
            </el-form-item>
            <el-form-item label="接口 Key">
              <el-input v-model="registerForm.key" autocomplete="off" />
            </el-form-item>
            <el-form-item label="角色">
              <el-select v-model="registerForm.role">
                <el-option :label="'普通'" :value="1" />
                <el-option :label="'管理员'" :value="2" />
              </el-select>
            </el-form-item>
          </el-form>
          <div style="text-align:right; margin-top:12px">
            <el-button @click="loginDialogVisible = false">取消</el-button>
            <el-button type="primary" @click="doRegister">注册并登录</el-button>
          </div>
        </div>
        <div style="margin-top:12px; display:flex; gap:8px; justify-content:center;">
          <el-button type="text" @click="loginMode = 'login'">登录</el-button>
          <el-button type="text" @click="loginMode = 'register'">注册</el-button>
        </div>
      </el-dialog>

    </div>
  </div>
</template>

<script setup>
import Splitter from './SplitBar.vue';
import { ref, onMounted, reactive, computed } from 'vue';
import { useRoute } from 'vue-router'
import { Notebook, DocumentChecked, Edit, Delete, Setting, User } from '@element-plus/icons-vue'
import { useRouter } from 'vue-router'
import { ElMessageBox, ElMessage } from 'element-plus'
import api from '../utils/api'
import { showAxiosErrorMessage, showErrorMessage } from '@/utils/utils';

const leftWidth = ref(200);

const router = useRouter()
const route = useRoute()

const activeMenu = computed(() => {
  const p = route.path || ''
  const q = route.query || {}
  if (p.startsWith('/articles')) {
    if (q.group) return `articles:group:${q.group}`
    return 'articles'
  }
  if (p.startsWith('/tasks')) {
    if (q.project) return `tasks:project:${q.project}`
    return 'tasks'
  }
  return ''
})

function isActive(root, type, id) {
  if (!id) return activeMenu.value === root
  return activeMenu.value === `${root}:${type}:${id}`
}

const projects = ref([])

const pageGroups = ref([])

async function fetchPageGroups() {
  try {
    const res = await api.get('/api/page_group_list', { params: { count: 100, page: 0 } })
    const j = res.data
    if (j && j.r === 0) {
      pageGroups.value = j.data || []
    } else {
      pageGroups.value = []
      showErrorMessage(ElMessage, j)
    }
  } catch (e) {
    showAxiosErrorMessage(ElMessage, e)
  }
}

async function fetchProjects() {
  try {
    const res = await api.get('/api/proj_list', { params: { count: 100, page: 0 } })
    const j = res.data
    if (j && j.r === 0) {
      projects.value = j.data || []
    } else {
      projects.value = []
      showErrorMessage(ElMessage, j)
    }
  } catch (e) {
    showAxiosErrorMessage(ElMessage, e)
  }
}

async function onAddProject() {
  try {
    const { value } = await ElMessageBox.prompt('输入项目名称', '新建项目', {
      confirmButtonText: '创建',
      cancelButtonText: '取消',
    })
    const name = (value || '').trim()
    const res = await api.post('/api/proj_insert', { project_name: name })
    const j = res.data
    if (j && j.r === 0) {
      ElMessage.success('创建成功')
      await fetchProjects()
    } else {
      showErrorMessage(ElMessage, j)
    }
  } catch (e) {
    showAxiosErrorMessage(ElMessage, e)
  }
}

async function onAddPageGroup() {
  try {
    const { value } = await ElMessageBox.prompt('输入页面组名称', '新建页面组', {
      confirmButtonText: '创建',
      cancelButtonText: '取消'
    })
    const name = (value || '').trim() || 'Untitled'
    const res = await api.post('/api/page_group_insert', { group_name: name })
    const j = res.data
    if (j && j.r === 0) {
      ElMessage.success('创建成功')
      await fetchPageGroups()
    } else {
      showErrorMessage(ElMessage, j)
    }
  } catch (e) {
    showAxiosErrorMessage(ElMessage, e)
  }
}

async function onEditPageGroup(g) {
  try {
    const { value } = await ElMessageBox.prompt('输入页面组名称', '编辑页面组', {
      confirmButtonText: '保存',
      cancelButtonText: '取消',
      inputValue: g.group_name || ''
    })
    const name = (value || '').trim()
    if (!name) {
      ElMessage.warning('名称不能为空')
      return
    }
    const res = await api.post('/api/page_group_update',
      { page_group_id: g.page_group_id, group_name: name })
    const j = res.data
    if (j && j.r === 0) {
      ElMessage.success('修改成功')
      await fetchPageGroups()
    } else {
      showErrorMessage(ElMessage, j)
    }
  } catch (e) {
    showAxiosErrorMessage(ElMessage, e)
  }
}

async function onDeletePageGroup(g) {
  try {
    await ElMessageBox.confirm(
      `确认删除页面组：${g.group_name} (${g.page_group_id})？`,
      '删除页面组', {
      confirmButtonText: '删除',
      cancelButtonText: '取消',
      type: 'warning'
    })
    const res = await api.post('/api/page_group_delete', { page_group_id: g.page_group_id })
    const j = res.data
    if (j && j.r === 0) {
      ElMessage.success('删除成功')
      await fetchPageGroups()
    } else {
      showErrorMessage(ElMessage, j)
    }
  } catch (e) {
    showAxiosErrorMessage(ElMessage, e)
  }
}

async function onEditProject(p) {
  try {
    const { value } = await ElMessageBox.prompt('输入项目名称', '编辑项目', {
      confirmButtonText: '保存',
      cancelButtonText: '取消',
      inputValue: p.project_name || '',
    })
    const name = (value || '').trim()
    if (!name) {
      ElMessage.warning('名称不能为空')
      return
    }
    const res = await api.post('/api/proj_update', { project_id: p.project_id, project_name: name })
    const j = res.data
    if (j && j.r === 0) {
      ElMessage.success('更新成功')
      await fetchProjects()
    } else {
      showErrorMessage(ElMessage, j)
    }
  } catch (e) {
    showAxiosErrorMessage(ElMessage, e)
  }
}

async function onDeleteProject(p) {
  try {
    await ElMessageBox.confirm(`确认删除项目：${p.project_name}？`, '删除项目', {
      confirmButtonText: '删除',
      cancelButtonText: '取消',
      type: 'warning',
    })
    const res = await api.post('/api/proj_delete', { project_id: p.project_id })
    const j = res.data
    if (j && j.r === 0) {
      ElMessage.success('删除成功')
      await fetchProjects()
    } else {
      showErrorMessage(ElMessage, j)
    }
  } catch (e) {
    showAxiosErrorMessage(ElMessage, e)
  }
}

function onDrag(movementX) {
  leftWidth.value += movementX;
}

function onMenuSelect(key) {
  if (key === 'articles') return router.push('/articles')
  if (key === 'tasks') return router.push('/tasks')

  const parts = key.split(':')
  if (parts.length === 3) {
    const [root, type, id] = parts
    if (root === 'articles' && type === 'group') {
      router.push({ path: '/articles', query: { group: id } })
    }
    if (root === 'tasks' && type === 'project') {
      router.push({ path: '/tasks', query: { project: id } })
    }
  }
}

onMounted(() => {
  fetchProjects()
  fetchPageGroups()
})

// Auth state and dialog for login/register
const auth = reactive({ loggedIn: false, username: '', role: 0 })
const loginDialogVisible = ref(false)
const loginMode = ref('login') // 'login' or 'register'

const loginForm = reactive({ user_name: '', password: '' })
const registerForm = reactive({ name: '', password: '', key: '', role: 1 })
const loggedIn = computed(() => auth.loggedIn)
const username = computed(() => auth.username)

function loadAuthFromStorage() {
  try {
    const s = localStorage.getItem('auth')
    if (s) {
      const t = JSON.parse(s)
      auth.loggedIn = true
      auth.username = t.user_name || t.name || ''
      auth.role = t.role || 0
    }
  } catch (e) {
    console.error(e)
  }
}

loadAuthFromStorage()

function openLoginDialog(mode) {
  loginMode.value = mode || 'login'
  loginDialogVisible.value = true
}

async function doLogin() {
  try {
    const res = await api.get('/api/login', { params: { user_name: loginForm.user_name, password: loginForm.password } })
    const j = res.data
    if (j && j.r === 0) {
      auth.loggedIn = true
      auth.username = loginForm.user_name
      auth.role = j.data?.role ?? 0
      localStorage.setItem('auth', JSON.stringify({ user_name: auth.username, role: auth.role }))
      ElMessage.success('登录成功')
      loginDialogVisible.value = false
    } else {
      showErrorMessage(ElMessage, j)
    }
  } catch (e) {
    showAxiosErrorMessage(ElMessage, e)
  }
}

async function doRegister() {
  try {
    const res = await api.post('/api/register', { name: registerForm.name, password: registerForm.password, key: registerForm.key, role: registerForm.role })
    const j = res.data
    if (j && j.r === 0) {
      ElMessage.success('注册成功，正在登录...')
      loginForm.user_name = registerForm.name
      loginForm.password = registerForm.password
      await doLogin()
    } else {
      showErrorMessage(ElMessage, j)
    }
  } catch (e) {
    showAxiosErrorMessage(ElMessage, e)
  }
}

function onSettings() {
  router.push('/settings')
}
</script>

<style scoped>
.main-page {
  display: flex;
  height: 100vh;
}

.sidebar {
  background: #1f2937;
  position: relative;
}

.sidebar-header {
  height: 60px;
}

.sidebar-footer {
  position: absolute;
  bottom: 0;
  left: 0;
  width: 100%;
  box-sizing: border-box;
}

.content {
  flex: 1;
  background: white;
}

.menu-item-row {
  display: flex;
  align-items: center;
  justify-content: space-between;
  width: 100%;
}

.menu-item-active {
  background: rgba(255, 255, 255, 0.04);
  border-left: 4px solid #3b82f6;
}

.menu-item-active .menu-item-name {
  font-weight: 700;
}

.menu-item-name {
  color: #fff;
  min-width: 0;
  overflow: hidden;
  text-overflow: ellipsis;
}

.menu-item-actions {
  display: flex;
  gap: 6px;
}

.menu-item-actions .el-button {
  max-width: 8px;
}

.menu-item-row .menu-item-actions {
  display: none;
}

.menu-item-row:hover .menu-item-actions {
  display: flex;
}

.edit-btn .el-icon {
  font-size: 14px;
  margin: 0;
  padding: 0;
}

.menu-footer {
  padding: 8px 12px;
}
</style>