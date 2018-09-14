using System.Collections.Generic;
using System.Threading.Tasks;

using Marktplaats.Models;

namespace Marktplaats.Data
{
    public class ProductService : RestService
    {
        public async Task<List<Product>> GetProducts(int page, int pageSize, string filterString, int filterCategoryId, bool personalOverview)
        {
            var uri = AppSettings["ApiUri.Products"] + "?page=" + page + "&pagesize=" + pageSize
                + "&filterString=" + filterString + "&filterCategoryId=" + filterCategoryId
                + "&personalOverview=" + personalOverview;
            return await Get<List<Product>>(uri, string.Empty);
        }

        public async Task<Product> GetProduct(int productId)
        {
            return await Get<Product>(AppSettings["ApiUri.Products"], productId);
        }

        public async Task<Image> GetThumbnail(int productId)
        {
            return await Get<Image>(AppSettings["ApiUri.ProductsThumbnail"], productId);
        }

        public async Task<List<Bid>> GetBids(int productId)
        {
            return await Get<List<Bid>>(AppSettings["ApiUri.ProductsBids"], productId);
        }

        public async Task<List<Image>> GetImages(int productId)
        {
            return await Get<List<Image>>(AppSettings["ApiUri.ProductsImages"], productId);
        }

        public async Task<Category> GetCategory(int productId)
        {
            return await Get<Category>(AppSettings["ApiUri.ProductsCategory"], productId);
        }

        public async Task<Product> AddProduct(Product product)
        {
            return await Post<Product>(AppSettings["ApiUri.Products"], product, string.Empty);
        }

        public async Task<Product> EditProduct(Product product)
        {
            return await Put<Product>(AppSettings["ApiUri.Products"], product, product.pid);
        }

        public async Task<Product> DeleteProduct(Product product)
        {
            return await Delete<Product>(AppSettings["ApiUri.Products"], product.id);
        }
    }
}
