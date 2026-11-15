// Supabase Storage through its REST API with the service key (server-side
// only; the key never leaves the function). A memory store backs the tests.

export interface Store {
  put(bucket: string, path: string, data: Uint8Array, contentType: string): Promise<void>;
  get(bucket: string, path: string): Promise<Uint8Array | null>;
  remove(bucket: string, path: string): Promise<void>;
  /** Signed URL the client PUTs the file to (valid for 2 hours). */
  signUpload(bucket: string, path: string): Promise<string>;
  publicUrl(bucket: string, path: string): string;
}

export class SupabaseStore implements Store {
  constructor(private url: string, private key: string) {}

  private headers(extra: Record<string, string> = {}) {
    return { apikey: this.key, Authorization: `Bearer ${this.key}`, ...extra };
  }

  private object(bucket: string, path: string) {
    return `${this.url}/storage/v1/object/${bucket}/${path.split("/").map(encodeURIComponent).join("/")}`;
  }

  async put(bucket: string, path: string, data: Uint8Array, contentType: string) {
    const res = await fetch(this.object(bucket, path), {
      method: "POST",
      headers: this.headers({ "Content-Type": contentType, "x-upsert": "true" }),
      body: data as Uint8Array<ArrayBuffer>,
    });
    if (!res.ok) throw new Error(`storage put ${res.status}: ${(await res.text()).slice(0, 200)}`);
    await res.body?.cancel();
  }

  async get(bucket: string, path: string) {
    const res = await fetch(this.object(bucket, path), { headers: this.headers() });
    if (res.status === 400 || res.status === 404) { await res.body?.cancel(); return null; }
    if (!res.ok) throw new Error(`storage get ${res.status}`);
    return new Uint8Array(await res.arrayBuffer());
  }

  async remove(bucket: string, path: string) {
    const res = await fetch(`${this.url}/storage/v1/object/${bucket}`, {
      method: "DELETE",
      headers: this.headers({ "Content-Type": "application/json" }),
      body: JSON.stringify({ prefixes: [path] }),
    });
    await res.body?.cancel();
  }

  async signUpload(bucket: string, path: string) {
    const res = await fetch(`${this.url}/storage/v1/object/upload/sign/${bucket}/${path.split("/").map(encodeURIComponent).join("/")}`, {
      method: "POST",
      headers: this.headers({ "Content-Type": "application/json" }),
      body: "{}",
    });
    if (!res.ok) throw new Error(`storage sign ${res.status}: ${(await res.text()).slice(0, 200)}`);
    const body = await res.json();
    if (!body?.url) throw new Error("storage sign: no url");
    return `${this.url}/storage/v1${body.url}`;
  }

  publicUrl(bucket: string, path: string) {
    return `${this.url}/storage/v1/object/public/${bucket}/${path.split("/").map(encodeURIComponent).join("/")}`;
  }
}

export class MemoryStore implements Store {
  files = new Map<string, Uint8Array>();
  constructor(private base = "https://storage.test") {}
  async put(bucket: string, path: string, data: Uint8Array, _contentType = "") { this.files.set(`${bucket}/${path}`, data); }
  async get(bucket: string, path: string) { return this.files.get(`${bucket}/${path}`) ?? null; }
  async remove(bucket: string, path: string) { this.files.delete(`${bucket}/${path}`); }
  async signUpload(bucket: string, path: string) { return `${this.base}/upload/${bucket}/${path}?token=test`; }
  publicUrl(bucket: string, path: string) { return `${this.base}/public/${bucket}/${path}`; }
}
