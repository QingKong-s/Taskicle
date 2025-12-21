import axios from 'axios'

const DEFAULT_BASE = process.env.VUE_APP_API_BASE || 'http://localhost:8080'

const api = axios.create({
  baseURL: DEFAULT_BASE,
  timeout: 10_000,
  headers: {
    'Content-Type': 'application/json',
  },
})

api.interceptors.request.use(
  (config) => {
    return config
  },
  (error) => Promise.reject(error)
)

api.interceptors.response.use(
  (response) => response,
  (error) => {
    return Promise.reject(error)
  }
)

export function setBaseURL(url) {
  api.defaults.baseURL = url
}

export default api
