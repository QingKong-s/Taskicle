<template>
  <el-dialog
    v-model="dialogVisible"
    title="访问控制"
    width="960px"
    destroy-on-close
    append-to-body
    class="permission-dialog"
    :close-on-click-modal="false"
    @open="handleDialogOpen"
    @close="handleDialogClose"
  >
    <div class="permission-container">
      <div class="header-section">
        <div class="entity-info" v-if="currentEntity.id">
          <div class="info-text">
            <div class="entity-name">{{ currentEntity.name }}</div>
            <div class="entity-meta">
              <el-tag size="small" effect="plain" class="mr-2">{{ currentEntity.typeLabel }}</el-tag>
              <span class="meta-item">ID: {{ currentEntity.id }}</span>
              <span class="meta-item ml-2" v-if="currentEntity.containerId && currentEntity.containerId > 0">
                父ID: {{ currentEntity.containerId }}
              </span>
            </div>
          </div>
        </div>
        <div class="entity-info empty" v-else>
          <div class="icon-wrapper placeholder">
            <el-icon :size="24"><InfoFilled /></el-icon>
          </div>
          <span class="text-gray-400 font-medium">请选择一个实体以配置权限</span>
        </div>

        <div class="header-actions">
          <el-dropdown trigger="click" @command="handleSelectSpecial">
            <el-button plain round>
              <el-icon class="mr-1"><Menu /></el-icon> 特殊容器
            </el-button>
            <template #dropdown>
              <el-dropdown-menu>
                <el-dropdown-item :command="{ id: -2, name: '页面组根容器', type: -2 }">
                  <el-icon><Folder /></el-icon> 页面组根容器
                </el-dropdown-item>
                <el-dropdown-item :command="{ id: -3, name: '项目根容器', type: -3 }">
                  <el-icon><Collection /></el-icon> 项目根容器
                </el-dropdown-item>
              </el-dropdown-menu>
            </template>
          </el-dropdown>
          
          <el-button type="primary" round @click="handleSelectEntity">
            <el-icon class="mr-1"><Search /></el-icon> 选择实体
          </el-button>
        </div>
      </div>

      <el-divider class="my-4" />

      <div class="content-section" v-loading="loading">
        <div class="table-toolbar">
          <span class="section-title">
            授权用户列表 
            <el-tag v-if="aclList.length" type="info" round size="small" class="ml-2">{{ aclList.length }}</el-tag>
          </span>
          
          <div class="tool-btns" v-if="currentEntity.id">
            <el-button plain size="small" type="warning" @click="handleAddGuest">
              <el-icon class="mr-1"><User /></el-icon> 添加访客
            </el-button>
            <el-button plain size="small" type="primary" @click="handleAddUser">
              <el-icon class="mr-1"><Plus /></el-icon> 添加用户
            </el-button>
          </div>
        </div>

        <el-table 
          :data="aclList" 
          style="width: 100%" 
          :header-cell-style="{ background: '#f8fafc', color: '#64748b' }"
        >
          <el-table-column label="用户 / 角色" min-width="180">
            <template #default="{ row }">
              <div class="user-cell">
                <el-avatar :size="32" :class="getAvatarClass(row.user_id)" shape="square">
                  {{ getAvatarText(row) }}
                </el-avatar>
                <div class="user-detail">
                  <div class="username-row">
                    <span v-if="row.user_id === ID_ADMIN" class="font-bold">系统管理员</span>
                    <span v-else-if="row.user_id === ID_GUEST" class="font-bold">访客 (Guest)</span>
                    <span v-else>{{ row.username || `用户 ${row.user_id}` }}</span>
                    
                    <el-tag v-if="hasBit(row.access, DbAccess.Owner)" size="small" type="danger" effect="dark" class="scale-tag ml-2">OWNER</el-tag>
                  </div>
                </div>
              </div>
            </template>
          </el-table-column>

          <el-table-column 
            v-for="perm in availablePermissions" 
            :key="perm.bit" 
            align="center"
            min-width="90"
          >
            <template #header>
              <el-tooltip :content="perm.desc || perm.name" placement="top">
                <span class="cursor-help">{{ perm.name }}</span>
              </el-tooltip>
            </template>
            <template #default="{ row }">
              <el-checkbox 
                :model-value="checkPermissionDisplay(row, perm.bit)"
                :disabled="isRowLocked(row)"
                @change="(val) => togglePermission(row, perm.bit, val)"
              />
            </template>
          </el-table-column>

          <el-table-column label="操作" width="70" align="center" fixed="right">
            <template #default="{ row }">
              <el-button 
                v-if="!isRowLocked(row)"
                type="danger" 
                link 
                class="delete-btn"
                @click="removeUserFromAcl(row)"
              >
                <el-icon><Delete /></el-icon>
              </el-button>
            </template>
          </el-table-column>
          
          <template #empty>
            <div class="py-10 text-center">
              <el-empty description="暂无授权用户" :image-size="80" />
            </div>
          </template>
        </el-table>
      </div>
    </div>
  </el-dialog>
</template>

<script setup>
import { ref, reactive, inject, computed, defineProps, defineEmits } from 'vue'
import { ElMessage, ElMessageBox } from 'element-plus'
import { 
  Search, Plus, Delete, User, Menu, 
  Folder, Collection, InfoFilled,
} from '@element-plus/icons-vue'
import api from '@/utils/api'

const DbAccess = {
  Owner: 1 << 0,
  ReadContent: 1 << 1,
  WriteContent: 1 << 2,
  Delete: 1 << 3,
  ReadChange: 1 << 4,
  WriteComment: 1 << 5,
  Rename: 1 << 6,
  CreateEntity: 1 << 7,
  DeleteEntity: 1 << 8
}

const ID_ADMIN = -5
const ID_GUEST = -4

const EntityType = {
  PageGroup: 1,
  Page: 2,
  Project: 3,
  Task: 4,
  User: 5,
  
  ContainerPageGroup: -2,
  ContainerProject: -3
}

const TYPE_KEYS = {
  [EntityType.PageGroup]: 'PageGroup',
  [EntityType.Page]: 'Page',
  [EntityType.Project]: 'Project',
  [EntityType.Task]: 'Task',
  [EntityType.ContainerPageGroup]: 'ContainerPageGroup',
  [EntityType.ContainerProject]: 'ContainerProject'
}

const PERM_CONFIG = {
  PageGroup: [
    { name: '访问', bit: DbAccess.ReadContent },
    { name: '更名', bit: DbAccess.Rename },
    { name: '删除', bit: DbAccess.Delete },
    { name: '创建页面', bit: DbAccess.CreateEntity, desc: '在组下创建页面' },
  ],
  Page: [
    { name: '访问', bit: DbAccess.ReadContent },
    { name: '修改', bit: DbAccess.WriteContent },
    { name: '更名', bit: DbAccess.Rename },
    { name: '删除', bit: DbAccess.Delete },
  ],
  Project: [
    { name: '访问', bit: DbAccess.ReadContent },
    { name: '更名', bit: DbAccess.Rename },
    { name: '删除', bit: DbAccess.Delete },
  ],
  Task: [
    { name: '修改', bit: DbAccess.WriteContent },
    { name: '评论', bit: DbAccess.WriteComment },
    { name: '删除', bit: DbAccess.Delete },
  ],
  ContainerPageGroup: [{ name: '创建组', bit: DbAccess.CreateEntity }],
  ContainerProject: [{ name: '创建项目', bit: DbAccess.CreateEntity }]
}

const props = defineProps({
  modelValue: {
    type: Boolean,
    required: true
  },
  initialEntity: {
    type: Object,
    default: null
  }
})

const emit = defineEmits(['update:modelValue'])

const dialogVisible = computed({
  get: () => props.modelValue,
  set: (val) => emit('update:modelValue', val)
})

const loading = ref(false)
const aclList = ref([])
const currentEntity = reactive({ 
  id: null, 
  name: '', 
  typeCode: 0,
  typeLabel: '',
  containerId: -1 
})

const openSearch = inject('openSearch', null)

const availablePermissions = computed(() => {
  const key = TYPE_KEYS[currentEntity.typeCode]
  return PERM_CONFIG[key] || []
})

const setEntity = (id, name, type, containerId = -1) => {
  currentEntity.id = id
  currentEntity.name = name
  currentEntity.typeCode = type
  currentEntity.containerId = containerId
  
  const labelMap = {
    [EntityType.PageGroup]: '页面组',
    [EntityType.Page]: '页面',
    [EntityType.Project]: '项目',
    [EntityType.Task]: '任务',
    [EntityType.ContainerPageGroup]: '系统容器',
    [EntityType.ContainerProject]: '系统容器'
  }
  currentEntity.typeLabel = labelMap[type] || '未知实体'
  
  loadAcl()
}

const handleDialogOpen = () => {
  if (props.initialEntity && props.initialEntity.id) {
    const { entity_id, name, type, container_id } = props.initialEntity
    setEntity(entity_id, name, type, container_id)
  } else {
    if (!currentEntity.id) {
      aclList.value = []
    }
  }
}

const handleDialogClose = () => {
  if (!currentEntity.id) {
    aclList.value = []
  }
}

const loadAcl = async () => {
  if (!currentEntity.id) {
    aclList.value = []
    return
  }
  
  loading.value = true
  try {
    const res = await api.get('/api/acl', { params: { entity_id: currentEntity.id } })
    aclList.value = res.data.data || []
  } catch (e) {
    console.error('权限列表加载失败:', e)
    ElMessage.error('权限列表加载失败')
    aclList.value = []
  } finally {
    loading.value = false
  }
}

const hasBit = (access, bit) => (access & bit) === bit

const checkPermissionDisplay = (row, bit) => {
  if (row.user_id === ID_ADMIN) return true
  if (hasBit(row.access, DbAccess.Owner)) return true
  return hasBit(row.access, bit)
}

function isRowLocked(row) {
  return row.user_id === ID_ADMIN || (row.access & DbAccess.Owner)
}

const togglePermission = async (row, bit, isChecked) => {
  try {
    const res = await api.post('/api/modify_acl', {
      user_id: row.user_id,
      entity_id: currentEntity.id,
      access: bit,
      is_remove: !isChecked
    })
    
    if (res.data?.r === 0) {
      if (isChecked) row.access |= bit
      else row.access &= ~bit
      if (bit === DbAccess.Owner) loadAcl()
    } else {
      ElMessage.error(res.data?.msg || '操作失败')
    }
  } catch (e) {
    ElMessage.error('网络请求失败')
  }
}

const addUserToAclApi = async (userId, username = '') => {
  username
  if (aclList.value.some(u => u.user_id === userId)) {
    ElMessage.warning('该用户已在列表中')
    return
  }
  
  loading.value = true
  try {
    const res = await api.post('/api/modify_acl_user', {
      user_id: userId,
      entity_id: currentEntity.id,
      is_remove: false
    })

    if (res.data?.r === 0) {
      ElMessage.success('添加成功')
      await loadAcl()
    } else {
      ElMessage.error(res.data?.msg || '添加失败')
    }
  } catch (e) {
    ElMessage.error('添加请求失败')
  } finally {
    loading.value = false
  }
}

const handleAddGuest = () => addUserToAclApi(ID_GUEST, 'Guest')

const removeUserFromAcl = (row) => {
  ElMessageBox.confirm(
    `确认移除用户 "${row.username || row.user_id}" 的所有权限？`,
    '警告',
    { type: 'warning' }
  ).then(async () => {
    loading.value = true
    try {
      const res = await api.post('/api/modify_acl_user', {
        user_id: row.user_id,
        entity_id: currentEntity.id,
        is_remove: true
      })
      if (res.data?.r === 0) {
        aclList.value = aclList.value.filter(u => u.user_id !== row.user_id)
        ElMessage.success('已移除')
      } else {
        ElMessage.error(res.data?.msg || '移除失败')
      }
    } catch (e) {
      ElMessage.error('操作失败')
    } finally {
      loading.value = false
    }
  }).catch(() => {
    /* */
  })
}

const handleSelectEntity = () => {
  if (!openSearch) return ElMessage.error('搜索服务不可用')
  
  openSearch({
    onSelect: (item) => {
      const validTypes = [EntityType.PageGroup, EntityType.Page, EntityType.Project, EntityType.Task]
      if (!validTypes.includes(item.type)) {
        if (item.type === EntityType.User) {
          ElMessage.error('不能选择"用户"作为权限配置的目标实体')
        } else {
          ElMessage.error('不支持的实体类型')
        }
        return
      }

      setEntity(item.entity_id, item.name, item.type, item.container_id)
    }
  })
}

const handleAddUser = () => {
  if (!openSearch) return ElMessage.error('搜索服务不可用')

  openSearch({
    onSelect: (item) => {
      if (item.type !== EntityType.User) {
        ElMessage.error('请选择一个用户，不能添加实体作为授权对象')
        return
      }
      
      addUserToAclApi(item.entity_id, item.name)
    }
  })
}

const handleSelectSpecial = (cmd) => {
  setEntity(cmd.id, cmd.name, cmd.type)
}

const getAvatarText = (row) => {
  if (row.user_id === ID_ADMIN) return 'A'
  if (row.user_id === ID_GUEST) return 'G'
  return (row.username || 'U')[0].toUpperCase()
}

const getAvatarClass = (uid) => {
  if (uid === ID_ADMIN) return 'bg-red-50 text-red-500'
  if (uid === ID_GUEST) return 'bg-amber-50 text-amber-500'
  return 'bg-blue-50 text-blue-500'
}
</script>

<style scoped>
.permission-container {
  padding: 0 4px;
}

.header-section {
  display: flex;
  justify-content: space-between;
  align-items: center;
}

.entity-info {
  display: flex;
  align-items: center;
  gap: 16px;
}

.info-text .entity-name {
  font-size: 18px;
  font-weight: 700;
  color: #1e293b;
  margin-bottom: 4px;
}

.info-text .entity-meta {
  display: flex;
  align-items: center;
  font-size: 13px;
  color: #94a3b8;
}

.meta-item {
  font-family: 'Menlo', 'Monaco', monospace;
  background: #f8fafc;
  padding: 0 4px;
  border-radius: 4px;
}

.table-toolbar {
  display: flex;
  justify-content: space-between;
  align-items: center;
  margin-bottom: 16px;
}

.section-title {
  font-weight: 600;
  color: #334155;
  font-size: 16px;
  display: flex;
  align-items: center;
}

.user-cell {
  display: flex;
  align-items: center;
  padding: 4px 0;
}

.user-detail {
  margin-left: 12px;
}

.username-row {
  font-weight: 500;
  color: #334155;
  display: flex;
  align-items: center;
}

.scale-tag {
  transform: scale(0.85);
  font-weight: 700;
}

.delete-btn:hover {
  background-color: #fee2e2;
}

.bg-red-50 { background-color: #fef2f2; color: #ef4444; }
.bg-amber-50 { background-color: #fffbeb; color: #f59e0b; }
.bg-blue-50 { background-color: #eff6ff; color: #3b82f6; }
.ml-2 { margin-left: 0.5rem; }
.mr-1 { margin-right: 0.25rem; }
.mr-2 { margin-right: 0.5rem; }
.my-4 { margin-top: 1rem; margin-bottom: 1rem; }
.cursor-help { cursor: help; border-bottom: 1px dashed #ccc; }
.py-10 { padding-top: 2.5rem; padding-bottom: 2.5rem; }
.text-center { text-align: center; }
.text-gray-400 { color: #9ca3af; }
.font-medium { font-weight: 500; }
.font-bold { font-weight: 700; }
</style>