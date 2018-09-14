using System;
using System.Collections.Generic;
using System.Linq;

using Microsoft.EntityFrameworkCore;

using MoreLinq;
using AutoMapper;

using MarktplaatsApi.Models;
using MarktplaatsApi.ViewModels;

namespace MarktplaatsApi.Services
{
    public class ProductRepository : CrudRepository<Product>, IDisposable
    {
        public ProductRepository(IMapper mapper) : base(mapper)
        {
        }

        public IEnumerable<Product> GetProductsByPage(int page, int pageSize, 
            string filterString, int filterCategoryId, bool personalOverview, string userId = "")
        {
            IEnumerable<Product> products = context.Products.Include(x => x.Category).Include(x => x.User);

            // If a personal overview is requested, filter based on the logged in user
            if(personalOverview)
            {
                products = products.Where(x => x.User != null && x.User.Id == userId);
            }

            // If a filter category is given, apply the filter
            if (filterCategoryId != -1)
            {
                products = products.Where(x => x.Category != null && x.Category.Id == filterCategoryId);
            }

            // If a filter string is given, apply this filter
            if (!string.IsNullOrEmpty(filterString))
            {
                var filterStringLower = filterString.ToLower();
                products = products.Where(x => x.Title.ToLower().Contains(filterStringLower) || 
                    x.Description.ToLower().Contains(filterStringLower));
            }

            // To get a page, skip pagenumer * pagesize, and read pagesize products
            // The list is ordered by id, placing the newest products at the top
            return products.OrderByDescending(x => x.Id).Skip(pageSize * page).Take(pageSize);
        }

        public IEnumerable<Image> GetImagesByProduct(int productId)
        {
            return context.Images.Where(e => e.Product.Id == productId).OrderBy(x => x.OrderPosition);
        }

        public IEnumerable<Bid> GetBidsByProduct(int productId)
        {
            // Bids are ordered by price, placing the highest bid at the top
            return context.Bids.Include(x => x.User).Where(e => e.Product.Id == productId).OrderByDescending(e => e.Price);
        }

        public Bid GetTopBidByProduct(int productId)
        {
            var bids = context.Bids.Include(x => x.User).Where(e => e.Product.Id == productId);

            if (bids == null || !bids.Any())
            {
                return null;
            }

            // Return the bid containing the highest price
            return bids.MaxBy(e => e.Price);
        }

        public bool IsTopBid(int productId, decimal price)
        {
            var topBid = GetTopBidByProduct(productId);
            return topBid == null || price > topBid.Price;
        }

        public Product Edit(int id, ProductViewModel productViewModel, string userId)
        {
            var product = FindById(productViewModel.Id, x => x.User);
            
            if(id != productViewModel.Id || product == null || product.User == null || product.User.Id != userId)
            {
                return null;
            }

            var category = context.Categories.FirstOrDefault(c => c.Id == productViewModel.CategoryId);

            if(category == null)
            {
                return null;
            }

            mapper.Map(productViewModel, product);
            product.Category = category;

            Edit(product);
            return product;
        }

        public Product Add(ProductViewModel productViewModel, string userId)
        {
            var category = context.Categories.FirstOrDefault(c => c.Id == productViewModel.CategoryId);
            var user = context.Users.FirstOrDefault(c => c.Id == userId);

            if (category == null || user == null)
            {
                return null;
            }

            var product = mapper.Map<Product>(productViewModel);
            product.Category = category;
            product.User = user;

            Add(product);
            return product;
        }

        public bool Remove(Product product, string userId)
        {
            if (product.User == null || product.User.Id != userId)
            {
                return false;
            }

            var images = GetImagesByProduct(product.Id);
            var imageRepository = new ImageRepository(mapper);
            foreach(var image in images)
            {
                imageRepository.DeleteImageFile(image);
            }

            Remove(FindById(product.Id));

            return true;
        }
    }
}