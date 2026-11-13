using System;
using System.IO;
using System.Net;
using System.Reflection;
using System.Runtime.Serialization;
using System.Runtime.Serialization.Json;
using System.Text;
using System.Threading.Tasks;
namespace StadiaStudio {
    [DataContract] public sealed class PublishedRelease {
        [DataMember(Name="tag_name")] public string Tag;
        [DataMember(Name="html_url")] public string Url;
        [DataMember(Name="url")] public string ApiUrl;
        [DataMember(Name="draft")] public bool Draft;
        [DataMember(Name="prerelease")] public bool Prerelease;
        public Version Version;
    }
    // StadiaLink Rev uses its own 1.0 version line. Only stable releases
    // published in this repository are accepted; a different repository, host,
    // pre-release or malformed tag is rejected before any version comparison.
    public static class UpdateChecker {
        public const string Repository="szninjacat/StadiaLink-Rev";
        public const string ReleasesUrl="https://github.com/"+Repository+"/releases/latest";
        public const string ApiUrl="https://api.github.com/repos/"+Repository+"/releases/latest";
        public static Version Current {get{return Assembly.GetExecutingAssembly().GetName().Version;}}
        public static PublishedRelease Parse(string json){
            PublishedRelease release;
            using(var stream=new MemoryStream(Encoding.UTF8.GetBytes(json)))release=(PublishedRelease)new DataContractJsonSerializer(typeof(PublishedRelease)).ReadObject(stream);
            Version version;Uri uri;Uri api;
            if(release==null||release.Draft||release.Prerelease||String.IsNullOrWhiteSpace(release.Tag)||
               (release.Tag[0]!='v'&&release.Tag[0]!='V')||
               !System.Version.TryParse(release.Tag.TrimStart('v','V'),out version)||
               !Uri.TryCreate(release.Url,UriKind.Absolute,out uri)||uri.Scheme!="https"||
               !String.Equals(uri.Host,"github.com",StringComparison.OrdinalIgnoreCase)||
               !uri.AbsolutePath.StartsWith("/"+Repository+"/releases/tag/",StringComparison.OrdinalIgnoreCase)||!String.IsNullOrEmpty(uri.UserInfo)||
               !Uri.TryCreate(release.ApiUrl,UriKind.Absolute,out api)||api.Scheme!="https"||
               !String.Equals(api.Host,"api.github.com",StringComparison.OrdinalIgnoreCase)||
               !api.AbsolutePath.StartsWith("/repos/"+Repository+"/releases/",StringComparison.OrdinalIgnoreCase))return null;
            release.Version=Normalize(version);return release;
        }
        public static Version Normalize(Version version){return new Version(version.Major,version.Minor,Math.Max(0,version.Build),Math.Max(0,version.Revision));}
        public static bool IsNewer(PublishedRelease release,Version current){return release!=null&&release.Version>Normalize(current);}
        public static Task<PublishedRelease> Check(){return Task.Run(()=>{
            ServicePointManager.SecurityProtocol|=SecurityProtocolType.Tls12;
            var request=(HttpWebRequest)WebRequest.Create(ApiUrl);
            request.UserAgent="StadiaLink-Rev/"+Current;request.Accept="application/vnd.github+json";request.Headers["X-GitHub-Api-Version"]="2022-11-28";
            request.Timeout=request.ReadWriteTimeout=10000;request.AllowAutoRedirect=false;
            using(var response=(HttpWebResponse)request.GetResponse())using(var stream=response.GetResponseStream())using(var reader=new StreamReader(stream)){
                char[] buffer=new char[4096];var body=new StringBuilder();int count;
                while((count=reader.Read(buffer,0,buffer.Length))>0){body.Append(buffer,0,count);if(body.Length>1024*1024)throw new InvalidDataException("Release response too large.");}
                return Parse(body.ToString());
            }
        });}
        public static void Verify(){
            const string page="https://github.com/szninjacat/StadiaLink-Rev/releases/tag/v1.0.10";
            const string api="https://api.github.com/repos/szninjacat/StadiaLink-Rev/releases/999";
            const string valid="{\"tag_name\":\"v1.0.10\",\"html_url\":\""+page+"\",\"url\":\""+api+"\"}";
            var release=Parse(valid);
            if(release==null||!IsNewer(release,new Version(1,0,0))||IsNewer(release,new Version(1,0,10,0))||IsNewer(release,new Version(2,0)))throw new Exception("Update version ordering failed.");
            if(!String.Equals(Repository,"szninjacat/StadiaLink-Rev")||!ApiUrl.Contains("/"+Repository+"/"))throw new Exception("Update channel points outside this project.");
            if(Parse(valid.Replace("github.com","example.org"))!=null||
               Parse(valid.Replace("/szninjacat/StadiaLink-Rev/releases/tag/","/other/StadiaLink/releases/tag/"))!=null||
               Parse(valid.Replace("/repos/szninjacat/StadiaLink-Rev/releases/","/repos/other/StadiaLink/releases/"))!=null||
               Parse(valid.Replace("v1.0.10","v1.0.10-beta"))!=null||
               Parse(valid.Replace("v1.0.10","1.0.10"))!=null||
               Parse(valid.Replace(",\"url\":\""+api+"\"",""))!=null||
               Parse(valid.Replace("}",",\"draft\":true}"))!=null||
               Parse(valid.Replace("}",",\"prerelease\":true}"))!=null)throw new Exception("Untrusted or out-of-channel release accepted.");
        }
    }
}
